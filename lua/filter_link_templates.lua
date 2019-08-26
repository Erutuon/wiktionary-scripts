#! /usr/bin/env lua53

local fn, cbor_file_path = ...
if type(fn) ~= "string" then
	error("expected a function body as argument")
end
fn = load([[
local link, title, template = ...
local name, parameters = template.name, template.parameters
local lang, term, alt, tr = link.lang, link.term, link.alt, link.tr
return ]] .. fn, "arg1")
	or assert(load(fn, "arg1"))

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

local cbor_file = cbor_file_path and io.open(cbor_file_path, "rb") or io.stdin
local cbor_next = require 'cn-cbor'.decode_next
local wrap, yield = coroutine.wrap, coroutine.yield

local template_parameters_mt = {
	__index = function (parameters, key)
		return rawget(parameters, tostring(key))
	end,
}

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

local cbor = assert(cbor_file:read 'a')
xpcall(function ()
	local prev_template
	for link, title, template in require 'iterate_templates'.iterate_links_raw(iterate_cbor_templates(cbor)) do
		if fn(link, title, template) then
			if template ~= prev_template then
				data_by_title[title]:insert(template.text)
			end
			prev_template = template
		end
	end
end, function (err)
	io.stderr:write(tostring(err), "\n", debug.traceback(), "\n")
end)