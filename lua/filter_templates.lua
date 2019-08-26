#! /usr/bin/env lua53

local fn, cbor_file_path = ...
if type(fn) ~= "string" then
	error("expected a function body as argument")
end
fn = load([[
local template, title = ...
local name, parameters, text = template.name, template.parameters, template.text
return ]] .. fn, "arg1")
	or assert(load(fn, "arg1"))

local data_by_title = require "data_by_title" "templates"

local cbor_file = cbor_file_path and io.open(cbor_file_path, "rb") or io.stdin
local cbor_next = require 'cn-cbor'.decode_next
local wrap, yield = coroutine.wrap, coroutine.yield

local Array_index
local template_parameters_mt = {
	__index = function (parameters, key)
		local val = rawget(parameters, tostring(key))
		if val == nil then
			if not Array_index then
				Array_index = require "mediawiki.array".__index
			end
			return Array_index(parameters, key)
		else
			return val
		end
	end,
	__len = function (parameters)
		return parameters:length()
	end,
}

setmetatable(_ENV, {
	__index = function (self, key)
		local val
		if key == "regex" then
			val = require "rure"
		elseif key == "Array" then
			val = require "mediawiki.array"
		end
		if val ~= nil then
			self[key] = val
			return val
		end
	end,
})

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
				yield(template, title)
			end
		end
	end)
end

local cbor = assert(cbor_file:read 'a')
xpcall(function ()
	for template, title in iterate_cbor_templates(cbor) do
		if fn(template, title) then
			data_by_title[title]:insert(template.text)
		end
	end
end, function (err)
	io.stderr:write(debug.traceback(tostring(err), 2), "\n")
end)