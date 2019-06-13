local iter = require "iterate_templates"

local rure = require "luarure"
-- U+0304 COMBINING MACRON , U+0306 COMBINING BREVE
-- U+0313 COMBINING COMMA ABOVE, U+0314 COMBINING REVERSED COMMA ABOVE
local regex = rure.new([[
\b(?:
	(?:[αεου]ι
	|[αεηοω]υ
	|[αεηιουω])
		[\u{0304}\u{0306}]?
			(?:[^ιυ\u{0304}\u{0306}\u{0313}\u{0314}]
			|\b)
)
]], "casei", "unicode", "space")

local decompose = require "lutf8proc".decomp

local count = 0
local max = type((...)) == "string" and tonumber(...) or math.huge

local data_by_title = require "data_by_title" "templates"

local function lacks_breathing(str)
	return str and str:sub(1, 1) ~= "-"
		and regex:is_match(decompose(str))
end

for link, title, template in iter.iterate_links(io.read 'a') do
	if link.lang == "grc" and (lacks_breathing(link.term) or lacks_breathing(link.alt)) then
		data_by_title[title]:insert(template.text)
		count = count + 1
		if count > max then
			break
		end
	end
end