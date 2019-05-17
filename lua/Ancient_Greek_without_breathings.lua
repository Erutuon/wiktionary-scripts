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
local max = math.huge

local Array = require "mediawiki.array"
local title_to_templates = setmetatable({}, {
	__index = function (self, key)
		local val = Array()
		self[key] = val
		return val
	end
})
for link, title, template in iter.iterate_links(io.read 'a') do
	if link.lang == "grc" and link.term and link.term:sub(1, 1) ~= "-"
	and regex:is_match(decompose(link.term)) then
		title_to_templates[title]:insert(template.text)
		count = count + 1
		if count > max then
			break
		end
	end
end

local case_insensitive_comp = require "casefold".comp

local cjson = require "cjson"

for title, templates in require "mediawiki.table".sortedPairs(title_to_templates, case_insensitive_comp) do
	print(cjson.encode { title = title, templates = templates })
end