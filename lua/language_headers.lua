local count = 0
local max = ... and (tonumber(...) or error("Expected number")) or math.huge

local header_pattern = require "header_pattern"
local data_by_title = require "data_by_title"("langs")
local language_name_to_code = require "Module:wiktionary_language_name_to_code"

return function (content, title)
	count = count + 1
	
	local pos = 1
	while true do
		local level, header_content
		level, header_content, pos = header_pattern:match(content, pos)
		if not level then
			break
		end
		
		if level == 2 then
			local code = language_name_to_code[header_content]
			
			if code then
				data_by_title[title]:insert(code)
			end
		end
	end
	
	return count < max
end