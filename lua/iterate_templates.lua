local parse_template = require "parse_template"

local function iterate_templates(content, title_start, template_start)
	title_start = title_start or '\1'
	template_start = template_start or '\n'
	local title_and_text_pattern =
		title_start .. '(.-)(' .. template_start .. '.-)%f[' .. title_start .. '\0]'
	local template_start_pattern = template_start .. '(){{'
	local count = 0
	
	return coroutine.wrap(function ()
		for title, text in content:gmatch(title_and_text_pattern) do
			for pos in text:gmatch(template_start_pattern) do
				local success, template = pcall(parse_template, text, pos)
				
				if success then
					count = count + 1
					template.count = count
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

local affix_template_names = list_to_set {
	"af", "affix", "circumfix", "compound", "confix", "pref", "prefix",
	"suf", "suffix",
}

local function if_not_empty(val)
	if val ~= "" then
		return val
	end
end

local function iterate_links(content, title_start, template_start, template_iterator)
	local template_iterator = template_iterator
		or iterate_templates(content, title_start, template_start)
	local count = 0
	
	return coroutine.wrap(function ()
		for template, title in template_iterator do
			local name, parameters = template.name, template.parameters
			if affix_template_names[name] then
				local language_code = parameters.lang or parameters[1]
				for i, link_target in ipairs(parameters), parameters, (parameters.lang and 1 or 2) - 1 do
					local list_parameter_index = parameters.lang and i or i - 1
					local link = {
						lang = if_not_empty(parameters["lang" .. list_parameter_index]) or language_code,
						term = link_target,
						alt = if_not_empty(parameters["alt" .. list_parameter_index]),
						id = if_not_empty(parameters["id" .. list_parameter_index]),
					}
					count = count + 1
					link.count = count
					coroutine.yield(link, title, template)
				end
			else
				local link
				if link_template_names[name] then
					link = {
						lang = parameters[1],
						term = parameters[2],
						id = if_not_empty(parameters.id)
					}
					if name == "t" or name == "t+" then
						-- Here parameter 3 is gender.
						link.alt = if_not_empty(parameters.alt)
					else
						-- Some templates, like {{l}} and {{m}} do not use the alt parameter,
						-- but it doesn't hurt to test for it.
						link.alt = if_not_empty(parameters[3]) or if_not_empty(parameters.alt)
					end
				elseif derivation_template_names[name] then
					link = {
						lang = parameters[2],
						term = parameters[3],
						alt = if_not_empty(parameters[4]) or if_not_empty(parameters.alt),
						id = if_not_empty(parameters.id)
					}
				end
				
				if link then
					count = count + 1
					link.count = count
					coroutine.yield(link, title, template)
				end
			end
		end
	end)
end

return { iterate_templates = iterate_templates, iterate_links = iterate_links }