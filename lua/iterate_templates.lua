local parse_template = require "parse_template"

local function iterate_templates(content, title_start, template_start)
	title_start = title_start or '\1'
	template_start = template_start or '\n'
	local pattern = title_start .. '(.-)(' .. template_start .. '.-)%f[' .. title_start .. '\0]'
	
	return coroutine.wrap(function ()
		for title, text in content:gmatch(pattern) do
			for pos in text:gmatch(template_start .. '(){{') do
				local success, template = pcall(parse_template, text, pos)
				
				if success then
					coroutine.yield(template, title)
				end
			end
		end
	end)
end

require "mediawiki"

local list_to_set = require "Module:table".listToSet

-- Templates in which the language code is in the first parameter
-- and the link target in the second.
local link_template_names = list_to_set {
	"m", "m-self", "m+", "langname-mention", "l", "l-self", "ll", "t", "t+", "cog", "cognate", "ncog",
	"noncognate"
}

-- Templates in which the language code is in the second parameter
-- and the link target in the third.
local derivation_template_names = list_to_set {
	"der", "derived", "inh", "inherited", "bor", "borrowed", "lbor",
	"learned borrowing", "cal", "calq", "calque", "clq",
}

local function iterate_links(content, title_start, template_start, template_iterator)
	local template_iterator = template_iterator
		or iterate_templates(content, title_start, template_start)
	
	return coroutine.wrap(function ()
		for template, title in template_iterator do
			local language_code, link_target
			if link_template_names[template.name] then
				language_code = template.parameters[1]
				link_target = template.parameters[2]
			elseif derivation_template_names[template.name] then
				language_code = template.parameters[2]
				link_target = template.parameters[3]
			end
			
			if language_code and link_target then
				coroutine.yield(language_code, link_target, title, template)
			end
		end
	end)
end

return { iterate_templates = iterate_templates, iterate_links = iterate_links }