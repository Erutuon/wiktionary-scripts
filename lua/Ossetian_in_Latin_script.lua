local iter = require "iterate_templates"
local rure = require "luarure"

local Latin = rure.new "\\p{Latn}"

require "mediawiki"
local Array = require "Module:array"
local matches = {}
setmetatable(matches, {
	__index = function (self, k)
		local val = Array()
		self[k] = val
		return val
	end
})

local uniname = require "uniname"
local function character_name(character)
	return uniname.from_codepoint(utf8.codepoint(character))
end

local count = 0
for link, title, template in iter.iterate_links(assert(io.read "a")) do
	if link.lang == "os" and link.term and Latin:is_match(link.term) then
		local data = {
			template = template.text,
			text = link.term,
			param = link.term_param,
			lang = link.lang,
		}
		
		count = count + 1
		
		local chars = Array()
		data.chars = chars
		for char in Latin:iter(link.term) do
			chars:insert { char = char, name = character_name(char) }
		end
		
		matches[title]:insert(data)
	end
end

io.stderr:write("count: ", count, "\n")

local sorted_pairs = require 'Module:table'.sortedPairs
local case_insensitive_comp = require "casefold".comp

local cjson = require 'cjson'
for title, templates in sorted_pairs(matches, case_insensitive_comp) do
	print(cjson.encode { title = title, templates = templates })
end