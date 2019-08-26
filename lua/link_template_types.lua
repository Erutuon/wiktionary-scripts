local M = {}

local list_to_set = require "Module:table".listToSet

-- Templates in which the language code is in the first parameter
-- and the link target in the second.
M.simple = list_to_set {
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
	"noncognate", "noncog", "ncog", "nc",
	"rebracketing",
	"t",
	"t+",
	"t-check",
	"t+check",
}

M.translation = {
	"t",
	"t+",
	"t-check",
	"t+check",
}

-- Templates in which the language code is in the second parameter
-- and the link target in the third.
M.foreign_derivation = list_to_set {
	"borrowed", "bor",
	"calque", "cal", "calq", "clq", "loan translation",
	"derived", "der",
	"inherited", "inh",
	"learned borrowing", "lbor",
	"orthographic borrowing", "obor",
	"phono-semantic matching", "psm",
	"semantic loan", "sl",
	"transliteration", "translit",
}

-- Templates that have multiple links and numbered parameters for alt and id.
M.multiple = list_to_set {
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
M.semantic_relation = list_to_set {
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

setmetatable(M, {
	__index = function (self, k)
		error("key " .. tostring(k) .. " not found")
	end,
})

return M