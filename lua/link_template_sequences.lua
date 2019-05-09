local lpeg = require 'lpeg'

for k, v in pairs(lpeg) do
	if type(k) == 'string' and k:find '^%u' then
		_ENV[k] = v
	end
end

local ws = S ' \t\n\r\v'

local link_separator = S ' \t' + P '-'

local count = 0
local count_up = function ()
	count = count + 1
	return true
end

require "mediawiki"

local languages, etymology_languages, families =
	require "Module:languages",
	require "Module:etymology languages",
	require "Module:families"

local function get_non_etymological(language_code)
	local lang = languages.getByCode(language_code)
		or etymology_languages.getByCode(language_code)
		or families.getByCode(language_code)
	
	if lang then
		lang = languages.getNonEtymological(lang)
		return lang and lang:getCode()
	end
end

local find_consecutive_link_templates = P {
	((V 'comment' + (1 - V 'consecutive_link_templates'))^1
		* C(Cmt(V 'consecutive_link_templates', count_up)))^1,
	-- (V 'comment' + C(V 'consecutive_link_templates') + 1)^1,
	consecutive_link_templates = V 'start_link_template'
		* Cmt(link_separator * Cb 'language_code' * V 'link_template' * Cb 'language_code',
			function (_, _, a, b)
				return a == b
			end)^1,
	start_link_template = P '{{' * ws^0
		* ((P 'm' * P 'ention'^-1) * ws^0
			* P '|' * ws^0 * Cg((R 'az' + P '-')^2, 'language_code') * ws^0
		+ (P 'cog' * P 'nate'^-1
		+  P 'n' * (P 'oncognate' + P 'cog')) * ws^0
			* P '|' * ws^0 * Cg((R 'az' + P '-')^2 / get_non_etymological, 'language_code') * ws^0
		+ (P 'der' * P 'ived'^-1
		+  P 'bor' * P 'rowed'^-1
		+  P 'inh' * P 'erited'^-1) * ws^0
			* P '|' * ws^0 * (R 'az' + P '-')^2 * ws^0
			* P '|' * ws^0 * Cg((R 'az' + P '-')^2 / get_non_etymological, 'language_code') * ws^0)
		* (P '|' * ((1 - (P '|' + P '{{' + P '}}'))^1 + V 'template'))^0 * P '}}',
	link_template = P '{{' * ws^0 * (P 'm' * P 'ention'^-1) * ws^0
		* P '|' * ws^0 * Cg((R 'az' + P '-')^2, 'language_code') * ws^0
		* (P '|' * ((1 - (P '|' + P '{{' + P '}}'))^1 + V 'template'))^0 * P '}}',
	template = P '{{' * (((1 - (P '{{' + P '}}'))^1 + V 'template'))^1 * P '}}',
	comment = P '<!--' * (1 - P '-->')^0 * P '-->',
}

local max = math.huge

return function (content, title)
	local matches = { find_consecutive_link_templates:match(content) }
	
	if type(matches[1]) == 'string' then
		io.write('\1', title, '\n')
		for _, match in ipairs(matches) do
			io.write('\2', match, '\n')
		end
	end
	
	return count < max
end