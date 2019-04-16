local lpeg = require "lpeg"

for k, v in pairs(lpeg) do
	if type(k) == "string" and k:find "^%u" then
		_ENV[k] = v
	end
end

local ws = S "\t\v\r\n "
local title_ws = ws + "_"

local function make_trimmer(following_pattern, to_trim, capture)
	to_trim = to_trim or ws
	
	local outer = (P(1) - (to_trim + following_pattern))^1
	
	local to_capture = outer * (to_trim^1 * outer)^0
	
	return P(to_trim)^0
		* (capture and C(to_capture) or to_capture)
		* to_trim^0
end

local function make_template_pattern(capture)
	local body = ((1 - (P "{{" + P "}}")) + V "template")^0
	
	local template = P {
		V 'template',
		template = P "{{" * title_ws^0
			* make_trimmer(P "|" + P "}}", title_ws, capture) -- not completely accurate
			* (capture and C(body) or body)
			* P "}}",
	}
	
	return capture and C(template) or template
end

local parameter_pattern = P {
	P "|" * ((ws^0
		* C((ws^0 * ((1 - (S "=|" + ws + V "not_in_params"))^1 + (V "template" + V "link")^1))^0)
			* ws^0 * P "=" * ws^0
			* C((ws^0 * ((1 - (P "|" + ws + V "not_in_params"))^1 + (V "template" + V "link")^1))^0)
			* ws^0)
		+ Cc(nil)
			* C(((1 - (P "|" + V "not_in_params"))^1 + (V "template" + V "link")^1)^0))
		* Cp(),
	link = P "[["
		* ((1 - (P "|" + P "[[" + P "]]" + P "{{" + P "}}"))^1 + V "template")^1
		* (P "|" * (1 - (P "[[" + P "]]"))^1)^-1
		* P "]]",
	template = make_template_pattern(false),
	not_in_params = P "{{" + P "}}" + P "[[",
	
}

local function log_conflicts(parameters, conflicts, key, value)
	if parameters[key] then
		conflicts[key] = conflicts[key] or {}
		table.insert(conflicts[key], parameters[key])
		table.insert(conflicts[key], value)
	end
end

local template_pattern = make_template_pattern(true)
local function parse_template(str)
	local template, title, body = template_pattern:match(str)
	
	if not template then
		error("The pattern did not match the string " .. str .. ".")
	end
	
	local pos
	local i = 0
	local parameters, conflicts = {}, {}
	local template = {
		text = template,
		name = title,
		body = body,
		parameters = parameters,
		conflicts = conflicts
	}
	while true do
		local name, value, new_pos
		-- name: string or nil
		-- value: string
		-- pos: integer
		name, value, new_pos = parameter_pattern:match(body, pos)
		
		if not value then
			if pos < #body then
				io.stderr:write("parameter pattern didn't match after position ", pos, "\n")
			end
			break
		else
			pos = new_pos
		end
		
		local key
		if name then
			key = tonumber(name) or name
		else
			i = i + 1
			key = i
		end
			
		log_conflicts(parameters, conflicts, key, value)
		
		parameters[key] = value
	end
	
	return template
end

return parse_template