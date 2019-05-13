local count = 0
local max = math.huge

require "windows.insert_lpeg"

local ws = S " \t\n\r\v"
local find_doublet_list = P {
	((1 - V "doublet_list")^0 * C(V "doublet_list"))^1,
	doublet_list = V "doublet" * V "subsequent"^1,
	doublet = P "{{" * ws^0 * P "doublet" * ws^0
		* P "|" * V "language_code"
		* (1 - P "}}")^1 * P "}}",
	subsequent = ws^0 * (P ","^1 + P "{{" * ws^0 * P "," * ws^0 * "}}")^-1 * ws^0 * P "and"^-1
		* ws^0 * (V "doublet" + V "mention"),
	mention = P "{{" * ws^0 * S "ml" * ws^0
		* P "|" * V "language_code"
		* ((1 - (P "{{" + P "}}")) + V "template")^1 * P "}}",
	template = P "{{" * ((1 - (P "{{" + P "}}"))^1 + V "template")^1 * P "}}",
	language_code = R("az", "--")^2,
}

local function print_matches(matches)
	local utf8proc = require "lutf8proc"
	
	local function casefold(str)
		return utf8proc.map(str, "casefold")
	end
	
	local cjson = require "cjson"
	
	table.sort(
		matches,
		function (a, b)
			local casefolded_a, casefolded_b =
				casefold(a.title), casefold(b.title)
			if casefolded_a ~= casefolded_b then
				return casefolded_a < casefolded_b
			else
				return a.title < b.title
			end
		end)
	
	for _, data in ipairs(matches) do
		print(cjson.encode(data))
	end
end

local matches = setmetatable({}, { __gc = print_matches })

local function save_matches(title, ...)
	if ... ~= nil then
		count = count + 1
		
		local data = {}
		data.title = title
		data.templates = {}
		for i = 1, select("#", ...) do
			data.templates[i] = select(i, ...)
		end
		table.insert(matches, data)
	end
end

return function (content, title)
	save_matches(title, find_doublet_list:match(content))
	
	return count < max
end