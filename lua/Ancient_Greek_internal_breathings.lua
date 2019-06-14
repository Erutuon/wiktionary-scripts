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
local limit = type((...)) == "string" and tonumber(...) or math.huge

local data_by_title = require "data_by_title" "templates"

local function has_internal_breathing(str)
	if str and str ~= "" then
		local decomposed = decompose(str)
		for captures in vowel_with_breathing:iter_captures(decomposed) do
			local preceding, vowel = captures[1], captures[2]
			if preceding ~= "" and letter_or_diacritic:is_match(preceding) then
				return true
			end
		end
	end
	
	return false
end

for link, title, template in iter.iterate_links(assert(io.read "a")) do
	if link.lang == "grc" and (has_internal_breathing(link.term) or has_internal_breathing(link.alt)) then
		data_by_title[title]:insert(template.text)
		
		count = count + 1
		
		if count > limit then
			break
		end
	end
end