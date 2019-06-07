local header_pattern = require "header_pattern"

local count = 0
local max = ... and tonumber(...) or math.huge

local name_to_code = require "mediawiki.languages.name_to_code"

function string:trim()
	return self:match("^%s*(.-)%s*$")
end

local Array = require "mediawiki.array"
local titles_by_language_code = {}
setmetatable(titles_by_language_code, {
	__index = function (self, k)
		local val = Array()
		self[k] = val
		return val
	end,
	__gc = function (self)
		local comp = require "casefold".comp
		for language_code, titles in pairs(self) do
			local file = assert(io.open(language_code .. ".txt", "wb"))
			table.sort(titles, comp)
			for _, title in ipairs(titles) do
				file:write(title, "\n")
			end
			file:close()
		end
	end
})

return function (content, title)
	local pos = 1
	while true do
		local header_level, header_content
		header_level, header_content, pos = header_pattern:match(content, pos)
		if not header_level then
			break
		end
		
		if header_level == 2 then
			local language_name = header_content:trim()
			local language_code = name_to_code[language_name]
			if language_code then
				titles_by_language_code[language_code]:insert(title)
				count = count + 1
			else
				io.stderr:write("Language name '", language_name, "' in [[", title, "]] not recognized.\n")
			end
		end
	end
	
	return count < max
end