require "mediawiki"
local Array = require "Module:array"

local key_for_data_key = {}
local file_key = {}

-- Enables table to print data as JSONL when garbage-collected.
local mt = {
	__index = function (self, k)
		local val = Array()
		self[k] = val
		return val
	end,
	__gc = function (self)
		local ok, msg = pcall(function()
			local json = require "json"
			local sorted_pairs = require "Module:table".sortedPairs
			local case_insensitive_comp = require "casefold".comp
			
			local data_key = rawget(self, key_for_data_key) or "data"
			self[key_for_data_key] = nil
			
			local file = rawget(self, file_key)
			self[file_key] = nil
			
			for title, data in sorted_pairs(self, case_insensitive_comp) do
				file:write(json.encode { title = title, [data_key] = data }, "\n")
			end
		end)
		
		if not ok then
			io.stderr:write("Error in __gc: ", msg, "\n")
		end
	end
}

return function (data_key, file)
	local data = setmetatable({}, mt)
	if data_key then
		data[key_for_data_key] = data_key
	end
	
	data[file_key] = file or io.stdout
			
	if not (type(data[file_key]) == "userdata" and type(data[file_key].write) == "function") then
		io.stderr:write("Invalid type for file: ", type(data[file_key]), "\n")
	end
	
	return data
end