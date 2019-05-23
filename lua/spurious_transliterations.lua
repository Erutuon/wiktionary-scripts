local iter = require "iterate_templates"
local templates_by_title = require "data_by_title"("templates")

local count = 0
local max = math.huge

local function some(map, func)
	for k, v in pairs(map) do
		if func(v, k) then
			return true
		end
	end
	
	return false
end

local Latin = require "luarure".new "^[\\p{Latin}\\p{Inherited}\\p{Common}]+$"

for template, title in iter.iterate_templates(assert(io.read 'a')) do
	local i = 1
	local parameters = template.parameters
	while parameters[i + 1] or parameters["tr" .. i] or parameters["alt" .. i] or parameters["t" .. i] do
		local term = parameters[i + 1]
		if term and term ~= "" and Latin:is_match(term) and parameters["tr" .. i] then
			templates_by_title[title]:insert(template.text)
			count = count + 1
			break
		end
		
		i = i + 1
	end
	
	if count > max then
		break
	end
end