local data_by_title = require "data_by_title" "templates"
local Array = require "mediawiki.array"

-- Matches a word containing only Latin, Common, and Inherited characters,
-- and at least one Latin character.
local Latin_regex = require "luarure".new "^[\\p{Latin}\\p{Common}\\P{Inherited}]*\\p{Latin}[\\p{Latin}\\p{Common}\\P{Inherited}]*$"

local function has_unwanted_transliteration(link)
	local term, alt, tr = link.term, link.alt, link.tr
	return (term and Latin_regex:is_match(term) or alt and Latin_regex:is_match(alt))
		and tr and tr ~= ""
end

local found = false
for link, title, template in require "iterate_templates".iterate_links(assert(io.read "a")) do
	if link.term:find("ciklo", 1, true) then
		io.stderr:write(template.text, "\n")
	end
	
	if has_unwanted_transliteration(link) then
		if not data_by_title[title]:contains(template.text) then
			data_by_title[title]:insert(template.text)
		end
	end
end