#! /usr/bin/env lua53

local iterate_templates = require "iterate_templates"

require "mediawiki"
local Array = require "Module:array"

local exclusions = Array {
	"None", "Zinh", "Zmth", "Zsym", "Zyyy", "Zzzz",
}:to_set()
local replacements = {
	Cyrs = "Cyrl", Geok = "Geor", Latinx = "Latn", ["as-Beng"] = "Beng", polytonic = "Grek",
}
local function script_filterer(script)
	return exclusions[script] == nil
end

local function script_mapper(script)
	return replacements[script] or script
end

local split_at_re = require "utils".split_at_re
local split_at_tab = split_at_re '"\t"'
local split_at_comma = split_at_re '", "'

local function remove_consecutive_duplicates(arr)
	local prev
	local new_arr = {}
	for _, v in ipairs(arr) do
		if prev ~= v then
			table.insert(new_arr, v)
		end
		prev = v
	end
	return Array(new_arr)
end

local function transform(script_codes)
	return remove_consecutive_duplicates(
		Array(split_at_comma(script_codes))
			:map(script_mapper)
			:sort()
			:filter(script_filterer))
		:concat ", "
end

local count = 0
local limit_arg = ...
local limit = type(limit_arg) == "string"
	and (tonumber(limit_arg) or error("invalid number '" .. tostring(limit_arg) .. "'"))
	or math.huge

local lines = {}
local reductions = 0
local empty_script_sets = 0
xpcall(function ()
	for line in io.lines() do
		if lines[line] == nil then
			local term, lang, scripts1, scripts2, scripts3, extra = split_at_tab(line)
			if extra then
				error("Too many commas in line " .. line)
			end
			scripts1, scripts2, scripts3 = transform(scripts1), transform(scripts2), transform(scripts3)
			if not (scripts1 == scripts2 and scripts2 == scripts3) then
				reductions = reductions + 1
				lines[line] = Array(term, lang, scripts1, scripts2, scripts3):concat "\t"
			else
				lines[line] = false
			end
			
			count = count + 1
			if count >= limit then
				break
			end
		end
	end
end, function (err)
	io.stderr:write(debug.traceback(tostring(err), 2), "\n")
end)

for _, line in require "Module:table".sortedPairs(lines, require "casefold".comp) do
	if line then
		print(line)
	end
end

io.stderr:write("left over: ", reductions, " / ", count,
	" (", ("%.3f"):format(reductions/count * 100), "%)")