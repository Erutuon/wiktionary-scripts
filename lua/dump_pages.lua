local max = type((...)) == "string" and tonumber(...) or math.huge
local count = 0

local data = setmetatable({}, {
	__gc = function (self)
		local ok, res = pcall(function ()
			local sorted_pairs = require "mediawiki.table".sortedPairs
			local case_insensitive_comp = require "casefold".comp
			local json = require "json"
			local stdout = io.stdout
			
			for title, content in sorted_pairs(self, case_insensitive_comp) do
				stdout:write(json.encode { title = title, content = content }, "\n")
			end
		end)
		io.stderr:write(tostring(ok), "\t", tostring(res), "\n")
	end,
})

return function (content, title, namespace)
	if title:find "^Template:R:" then
		data[title] = content
		count = count + 1
	end
	return count < max
end