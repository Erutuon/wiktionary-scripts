#! /usr/bin/env lua53

local inspect = require "inspect"

local function get_args_at_index(args, arg_keys, i)
	return require "mediawiki.fun".fold(
		function (accum, arg)
			local value = args[arg] and args[arg][i]
			if value ~= "" then
				accum[arg] = value
			end
			return accum
		end,
		arg_keys,
		{})
end

local function gather_args(args)
	local gathered_args = {}
	
	-- print(inspect(args))
	
	for k, v in pairs(args) do
		local name, number
		k = tonumber(k) or k
		if type(k) == "string" then
			name, number = k:match("(%a+)(%d*)")
			-- If number is empty, set it to 1.
			number = tonumber(number) or 1
		elseif type(k) == "number" then
			if k >= 2 then
				name = "term"
				number = k - 1
			end
		end
		if name and number then
			local arg_table = gathered_args[name]
			if not arg_table then
				arg_table = {}
				gathered_args[name] = arg_table
			end
			arg_table[number] = v
			arg_table.max = math.max(number, arg_table.max or 0)
		end
	end
	return gathered_args
end

local function get_labels(args)
	local gathered_args = gather_args(args)
	-- print(inspect(gathered_args))
	
	local term_parameters = { "term", "alt", "id", "tr", "ts", "t", "pos", "g" }

	local prev = false
	local use_semicolon = false
	
	local i = 1
	local prev = true
	while true do
		local term_data = get_args_at_index(gathered_args, term_parameters, i)
		if not next(term_data) then
			break
		end
		
		-- print(inspect(term_data))
		
		i = i + 1
	end
	
	return gathered_args.term
		and table.move(gathered_args.term, i + 1, gathered_args.term.max, 1, {})
		or {}
end

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

local cbor_file = cbor_file_path and io.open(cbor_file_path, "rb") or io.stdin
local cbor = assert(cbor_file:read 'a')
local label_counts = {}

xpcall(function ()
	for template, title in iterate_cbor_templates(cbor) do
		if template.parameters[1] == "enm" then
			local labels = get_labels(template.parameters)
			for _, label in ipairs(labels) do
				if label == "" then
					io.stderr:write("empty label at " .. title .. "\n")
				else
					print(title, label)
				end
				label_counts[label] = (label_counts[label] or 0) + 1
			end
		end
	end
end, function (err)
	io.stderr:write(debug.traceback(tostring(err), 2), "\n")
end)

for k, v in require "mediawiki.table".sortedPairs(label_counts) do
	print(k, v)
end