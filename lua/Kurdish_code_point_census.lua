local count = 0
local limit = ... and tonumber((...)) or math.huge

local counts_by_lang = setmetatable({}, {
	__index = function (self, key)
		local value = require "counter"()
		self[key] = value
		return value
	end
})

local heh_ZWNJ = "ه‌"
for link, title, template in require "iterate_templates".iterate_links(io.read "a") do
	local lang = link.lang
	local term = link.term
	if term and term ~= "" and (lang == "ku" or lang == "ckb" or lang == "lki" or lang == "kmr" or lang == "sdh") then
		local counter = counts_by_lang[lang]
		for _, code_point in utf8.codes(link.term) do
			counter:count(code_point)
		end
		
		-- ARABIC LETTER HEH, ZERO WIDTH NON-JOINER
		for characters in link.term:gmatch(heh_ZWNJ) do
			counter:count(characters)
		end
	end
end

require "mediawiki"
local eprint = require "utils".eprint
for lang, counter in pairs(counts_by_lang) do
	-- Force the JSON module to output an object.
	local to_be_encoded = { lang = lang, counts = {} }
	for item, count in pairs(counter) do
		local characters
		if type(item) == "number" then
			characters = utf8.char(item)
		else
			characters = item
		end
		to_be_encoded.counts[characters] = count
	end
	print(require "json".encode(to_be_encoded))
	
	local _, err = pcall(function ()
		eprint(("%s (%s)"):format(lang, require "Module:languages/alldata"[lang][1]))
		for letter in ("هہھە"):gmatch(utf8.charpattern) do
			if to_be_encoded.counts[letter] then
				eprint(("%s: %d"):format(letter, to_be_encoded.counts[letter]))
			end
		end
		
		if to_be_encoded.counts[heh_ZWNJ] then
			eprint(("%s: %d"):format(heh_ZWNJ, to_be_encoded.counts[heh_ZWNJ]))
		end
	end)
	
	eprint(err)
	
	--[[
	for code_point, count in require "mediawiki.table".sortedPairs(counter) do
		print(("U+%04X %s: %d"):format(code_point, require "uniname".from_codepoint(code_point), count))
	end
	--]]
end