#! /usr/bin/env lua53

local template_names = table.pack(...)

if #template_names == 0 then
	local file = assert(io.open("link_templates.txt", "rb"))
	template_names = {}
	for template_name in file:read "a":gmatch "[^\n]+" do
		table.insert(template_names, template_name)
	end
end

local data_by_title = require "data_by_title" "terms"

local template_parameters_mt = {
	__index = function (parameters, key)
		return rawget(parameters, tostring(key))
	end,
}

local cbor_next = require "cn-cbor".decode_next
local wrap, yield = coroutine.wrap, coroutine.yield

local function iterate_cbor_templates(cbor)
	local pos = 1
	return wrap(function ()
		while true do
			local data
			pos, data = cbor_next(cbor, pos)
			if pos == nil then
				return
			end
			local title, templates = data.title, data.templates
			for _, template in ipairs(templates) do
				template.parameters = setmetatable(template.parameters, template_parameters_mt)
				coroutine.yield(template, title)
			end
		end
	end)
end

local rure = require "luarure"
local regexes = {
	["iir-pro"] = rure.new(require "lutf8proc".comp(table.concat({"ĵ", "ĉ"}, "|"))),
	["ira-pro"] = rure.new(require "lutf8proc".comp(table.concat({"ĵ", "ĉ", "ȷ́", "ć"}, "|"))),
}
local function add_match(title, link, param, name)
	if link[param] and regexes[link.lang]:is_match(link[param])then
		data_by_title[title]:insert {
			lang = link.lang,
			lang_param = link.lang_param,
			-- text = link[param],
			text_param = link[param .. "_param"],
			name = name,
		}
	end
end

xpcall(function ()
	for _, template_name in ipairs(template_names) do
		local prev_template
		local filepath = "cbor/" .. template_name .. ".cbor"
		local file = io.open(filepath, "rb")
		if file then
			for link, title, template in require "iterate_templates".iterate_links_raw(iterate_cbor_templates(file:read "a")) do
				if link.lang == "ira-pro" or link.lang == "iir-pro" then
					add_match(title, link, "term", template.name)
					add_match(title, link, "alt", template.name)
				end
			end
		else
			io.stderr:write(filepath .. " not found\n")
		end
	end
end, function (err)
	io.stderr:write(tostring(err), "\n", debug.traceback(), "\n")
end)