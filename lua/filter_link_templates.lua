#! /usr/bin/env lua53

local fn = ...
local template_names = table.pack(select(2, ...))

io.stderr:write(table.concat(template_names, ", "), "\n")

if #template_names == 0 then
	local file = assert(io.open("link_templates.txt", "rb"))
	template_names = {}
	for template_name in file:read "a":gmatch "[^\n]+" do
		table.insert(template_names, template_name)
	end
end

if type(fn) ~= "string" then
	error("expected a function body as argument")
end

-- fn should be either an expression or a part of a block that contains return statements.
fn = load([[
local link, title, template = ...
local name, parameters = template.name, template.parameters
local lang, term, alt, tr = link.lang, link.term, link.alt, link.tr
return ]] .. fn, "arg1")
	or assert(load([[
local link, title, template = ...
local name, parameters = template.name, template.parameters
local lang, term, alt, tr = link.lang, link.term, link.alt, link.tr
]] .. fn, "arg1"))

local rure
_ENV.regex = setmetatable({}, {
	__call = function (self, str)
		local val = rawget(self, str)
		if val then
			return val
		end
		rure = require "luarure"
		val = rure.new(str)
		rawset(self, str, val)
		return val
	end,
})
_ENV.utf8proc = setmetatable({}, {
	__index = function (self, key)
		local utf8proc = require "lutf8proc"
		_ENV.utf8proc = utf8proc
		return utf8proc[key]
	end,
})

local data_by_title = require "data_by_title" "templates"

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

xpcall(function ()
	for _, template_name in ipairs(template_names) do
		local prev_template
		local filepath = "cbor/" .. template_name .. ".cbor"
		local file = io.open(filepath, "rb")
		if file then
			for link, title, template in require "iterate_templates".iterate_links_raw(iterate_cbor_templates(file:read "a")) do
				if fn(link, title, template) then
					if template ~= prev_template then
						local to_remove = {"count"}
						for k, v in pairs(link) do
							if k:find "param" then
								table.insert(to_remove, k)
							end
						end
						for _, k in ipairs(to_remove) do
							link[k] = nil
						end
						local record = {template = template, link = link}
						data_by_title[title]:insert(record)
					end
					prev_template = template
				end
			end
		else
			io.stderr:write(filepath .. " not found\n")
		end
	end
end, function (err)
	io.stderr:write(tostring(err), "\n", debug.traceback(), "\n")
end)
