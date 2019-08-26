#! /usr/bin/env lua53

local fn, path = ...
if type(fn) ~= "string" then
	error("expected a function body as argument")
end
fn = load([[
local transcription, lang, title = ...
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

local data_by_title = require "data_by_title" "templates"

local cbor_file_path = path or "cbor/IPA.cbor"
local cbor_file = io.open(cbor_file_path, "rb") or io.stdin

xpcall(function ()
	for _, data in require 'cn-cbor'.iter(assert(cbor_file:read 'a')) do
		local title, templates = data.title, data.templates
		for _, template in pairs(templates) do
			local parameters = template.parameters
			local language = parameters["lang"] or parameters["1"]
			for k, v in pairs(parameters) do
				local number_k = tonumber(k)
				if number_k and (parameters["lang"] or number_k > 1)
				and fn(v, language, title) then
					data_by_title[title]:insert(template.text)
				end
			end
		end
	end
end, function (err)
	io.stderr:write(tostring(err), "\n", debug.traceback(), "\n")
end)