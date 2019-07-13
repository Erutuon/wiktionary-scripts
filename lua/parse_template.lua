require "insert_lpeg"

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
		* C((ws^0 * (V "constructions"^1 + (1 - (S "=|" + ws + V "not_in_params"))^1))^0)
			* ws^0 * P "=" * ws^0
			* C((ws^0 * (V "constructions"^1 + (1 - (P "|" + ws + V "not_in_params"))^1))^0)
			* ws^0)
		+ Cc(nil)
			* C((V "constructions"^1 + (1 - (P "|" + V "not_in_params"))^1)^0))
		* Cp(),
	constructions = V "comment" + V "template" + V "link" + V "html",
	link = P "[["
		* ((1 - (P "|" + P "[[" + P "]]" + P "{{" + P "}}"))^1 + V "template")^1
		* (P "|" * (1 - (P "[[" + P "]]"))^1)^-1
		* P "]]",
	-- Assumes no nested tags with the same name!
	html = V "br"
		+ P "<" * Cg(V "html_identifier", "tag_name")
		  * (ws^0 * V "attribute"^0 * ws^0 * P ">" * (1 - V "close_tag")^0 * V "close_tag")
			 + ws^0 * "/>",
	close_tag = P "</"
		* Cmt(C(V "html_identifier") * Cb "tag_name",
			function (_, _, close, open)
				return close == open
			end)
		* ws^0 * P ">",
	comment = P "<!--" * (1 - P "-->")^0 * P "-->",
	br = P "<br" * ws^0 * P "/"^-1 * P">",
	attribute = (V "html_identifier" * P "=" * (P '"' * (1 - P '"')^0 * P '"' + (1 - ws)^0)),
	html_identifier = R("az", "AZ")^1,
	template = make_template_pattern(false),
	not_in_params = P "{{" + P "}}" + P "[[" + P "<!--",
	
}

local function log_conflicts(parameters, conflicts, key, value)
	if parameters[key] then
		conflicts[key] = conflicts[key] or {}
		table.insert(conflicts[key], parameters[key])
		table.insert(conflicts[key], value)
	end
end

local template_pattern = make_template_pattern(true)
local function parse_template(str, pos, all_string_parameter_names)
	local template, title, body = template_pattern:match(str, pos)
	
	if not template then
		error("The template pattern did not match the string " .. str:sub(pos, (pos or 1) + 100) .. ".")
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
			if (pos or 0) < #body then
				io.stderr:write("parameter pattern didn't match after position ", pos,
					": ", body:sub(pos) "\n")
			end
			break
		else
			pos = new_pos
		end
		
		local key
		if name then
			key = name
			
			if not all_string_parameter_names then
				key = tonumber(name) or name
			end
		else
			i = i + 1
			key = i
			
			if all_string_parameter_names then
				key = tostring(key)
			end
		end
			
		log_conflicts(parameters, conflicts, key, value)
		
		parameters[key] = value
	end
	
	return template
end

return parse_template