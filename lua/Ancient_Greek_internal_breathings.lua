local iter = require "iterate_templates"

local decompose = require "lutf8proc".decomp

local rure = require "luarure"
-- U+0313 COMBINING COMMA ABOVE, U+0314 COMBINING REVERSED COMMA ABOVE
local rough_or_smooth_breathing = rure.new "[\u{313}\u{314}]"
local vowel_with_breathing = rure.new([[
(.??)(
	(?:[αεου]ι       # iota diphthong
	|[αεηοω]υ        # upsilon diphthong
	|[αεηιουω])      # single vowel
	
	\pM*             # diacritic
	
	[\u{313}\u{314}] # breathing
)
]], "casei", "unicode", "space")
local letter_or_diacritic = rure.new "[\\pL\\pM-]"

local count = 0
local max = math.huge

local matches = {}
setmetatable(matches, {
	__index = function (self, k)
		local val = require 'Module:array'()
		self[k] = val
		return val
	end
})

for link, title, template in iter.iterate_links(assert(io.read "a")) do
	if link.lang == "grc" and link.term and link.term ~= "" then
		local decomposed = decompose(link.term)
		for captures in vowel_with_breathing:iter_captures(decomposed) do
			local preceding, vowel = captures[1], captures[2]
			if preceding ~= "" and letter_or_diacritic:is_match(preceding) then
				matches[title]:insert(template.text)
				
				count = count + 1
			end
		end
		
		if count > max then
			break
		end
	end
end

local sorted_pairs = require 'Module:table'.sortedPairs
local case_insensitive_comp = require "casefold".comp

local cjson = require 'cjson'
for title, templates in sorted_pairs(matches, case_insensitive_comp) do
	print(cjson.encode { title = title, templates = templates })
end