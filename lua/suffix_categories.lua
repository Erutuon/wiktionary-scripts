local rure = require "luarure"

local regex = rure.new "\\[\\[[ _]*(?i:Cat(?:egory)?)[ _]*:[ \n_]*([^\\|\\]\n]+?words[ _]*(?:circum|in|inter|pre|suf)fixed[ _]*with[^\\|\\]\n]+?)[ _]*(?:\\|[^\\]\n]+)?\\]\\]"

local count = 0
local max = ... and (tonumber(...) or error("invalid number '" .. tostring(...) .. "'"))
	or math.huge

local data_by_title = require "data_by_title"("templates")

return function (content, title)
	for captures in regex:iter_captures(content) do
		data_by_title[title]:insert(captures[1])
		count = count + 1
	end
	
	return count < max
end