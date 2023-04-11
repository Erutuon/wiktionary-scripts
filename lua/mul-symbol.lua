#! /usr/bin/env lua53

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

local data_by_title = require "data_by_title" "templates"

xpcall(function ()
	require "mediawiki"
	local mul = require "Module:languages".getByCode "mul"
	local find_best_script = require "Module:scripts".findBestScript
	local function get_script(text)
		assert(text)
		assert(mul and mul.getScripts)
		return find_best_script(text, mul)
	end
	
	local template_name = "mul-symbol"
	local filepath = "cbor/" .. template_name .. ".cbor"
	local cbor_file = assert(io.open(filepath, "rb"))
	local cbor = assert(cbor_file:read 'a')
	
	for template, title in iterate_cbor_templates(cbor) do
		local text = template.parameters.head or title
		local script = get_script(text):getCode()
		if script == "None" then
			print(title, template.text, text, script)
		end
	end
end, function (err)
	io.stderr:write(debug.traceback(tostring(err), 2), "\n")
end)