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
	"back-formation", "back-form", "bf",
	"clipping",
	"cognate", "cog",
	"descendant", "desc",
	"deverbal",
	"l", "link",
	"l-self",
	"langname-mention", "m+",
	"ll",
	"m", "mention",
	"m-self",
	"noncognate", "ncog", "nc",
	"rebracketing",
	"semantic loan", "sl",
	"t",
	"t+",
	"t-check",
}

-- Templates in which the language code is in the second parameter
-- and the link target in the third.
local foreign_derivation_template_names = list_to_set {
	"borrowed", "bor",
	"calque", "cal", "calq", "clq", "loan translation",
	"derived", "der",
	"inherited", "inh",
	"learned borrowing", "lbor",
	"orthographic borrowing", "obor",
	"phono-semantic matching", "psm",
	"transliteration", "translit",
}

-- Templates that have multiple links and numbered parameters for alt and id.
local multiple_link_template_names = list_to_set {
	"affix", "af",
	"blend", "blend of",
	"circumfix",
	"compound",
	"confix",
	"doublet",
	"prefix", "pref",
	"suffix", "suf",
}

-- The same as the above, except that there may be a series of Thesaurus links
-- at the end of the numbered parameters, which should be ignored.
local semantic_relation_template_names = list_to_set {
	"antonyms", "antonym", "ant",
	"coordinate terms", "cot",
	"holonyms",
	"hypernyms", "hyper",
	"hyponyms", "hypo",
	"imperfectives", "impf",
	"meronyms",
	"perfectives", "pf",
	"synonyms", "syn",
	"troponyms",
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
			if multiple_link_template_names[name] then
				local language_code = if_not_empty(parameters.lang) or if_not_empty(parameters[1])
				local language_param = if_not_empty(parameters.lang) and "lang" or 1
				for i, link_target in ipairs(parameters), parameters, (parameters.lang and 1 or 2) - 1 do
					local list_parameter_index = parameters.lang and i or i - 1
					local link = {
						lang = if_not_empty(parameters["lang" .. list_parameter_index]) or language_code,
						lang_param = if_not_empty(parameters["lang" .. list_parameter_index])
							and "lang" .. list_parameter_index or language_param,
						term = link_target,
						term_param = i,
						alt = if_not_empty(parameters["alt" .. list_parameter_index]),
						alt_param = "alt" .. list_parameter_index,
						id = if_not_empty(parameters["id" .. list_parameter_index]),
						id_param = "id" .. list_parameter_index,
						tr = if_not_empty(parameters["tr" .. list_parameter_index]),
						tr_param = "tr" .. list_parameter_index,
					}
					
					-- This only works for languages that use hyphen to indicate an affix,
					-- and for non-reconstructed suffixes. Need more complex rules for
					-- Arabic and reconstructed languages.
					-- The suffix, prefix, and confix templates don't use the lang parameter.
					if (name == "suffix" or name == "suf") and i >= 3 then
						link.raw_term = link.term
						link.term = "-" .. link.raw_term
					elseif (name == "prefix" or name == "pref") and if_not_empty(parameters[i + 1]) then
						link.raw_term = link.term
						link.term = link.raw_term .. "-"
					elseif name == "confix" or name == "con" then
						if i == 2 then
							link.raw_term = link.term
							link.term = link.raw_term .. "-"
						elseif i == #parameters then
							link.raw_term = link.term
							link.term = "-" .. link.raw_term
						end
					end
					
					count = count + 1
					link.count = count
					
					coroutine.yield(link, title, template)
				end
			elseif semantic_relation_template_names[name] then
				local lang = parameters[1]
				local i = 2
				while (parameters[i] or parameters["alt" .. i] or parameters["id" .. i])
				and not (parameters[i] and parameters[i]:find "^Thesaurus:") do
					local link = {
						lang = lang,
						lang_param = 1,
						term = if_not_empty(parameters[i]),
						term_param = i,
						alt = if_not_empty(parameters["alt" .. i]),
						alt_param = "alt" .. i,
						id = if_not_empty(parameters["id" .. i]),
						id_param = "id" .. i,
						tr = if_not_empty(parameters["tr" .. i]),
						tr_param = "tr" .. i,
					}
					
					count = count + 1
					link.count = count
					
					coroutine.yield(link, title, template)
					i = i + 1
				end
			else
				local link
				if link_template_names[name] then
					link = {
						lang = parameters[1],
						lang_param = 1,
						term = parameters[2],
						term_param = 2,
						id = if_not_empty(parameters.id),
						id_param = "id",
					}
					if name == "t" or name == "t+" then
						-- Here parameter 3 is gender.
						link.alt = if_not_empty(parameters.alt)
						link.alt_param = "alt"
					else
						-- Some templates, like {{l}} and {{m}} do not use the alt parameter,
						-- but it doesn't hurt to test for it.
						link.alt = if_not_empty(parameters[3]) or if_not_empty(parameters.alt)
						link.alt_param = if_not_empty(parameters[3]) and 3 or "alt"
					end
				elseif foreign_derivation_template_names[name] then
					link = {
						lang = parameters[2],
						lang_param = 2,
						term = parameters[3],
						term_param = 3,
						alt = if_not_empty(parameters[4]) or if_not_empty(parameters.alt),
						alt_param = if_not_empty(parameters[4]) and 4 or "alt",
						id = if_not_empty(parameters.id),
						id_param = "id",
					}
				end
				
				link.tr = if_not_empty(parameters.tr)
				link.tr_param = "tr"
				
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