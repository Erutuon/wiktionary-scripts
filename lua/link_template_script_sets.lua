#! /usr/bin/env lua53

local count = 0
local first_arg = ...
local limit_arg = first_arg
local limit = type(limit_arg) == "string" and tonumber(limit_arg) or math.huge
local template_names = table.pack(select(2, ...))

if #template_names == 0 then
	local file = assert(io.open("link_templates.txt", "rb"))
	template_names = {}
	for template_name in file:read "a":gmatch "[^\n]+" do
		table.insert(template_names, template_name)
	end
end

require "mediawiki"
local script_sets = require "Module:array"()
local get_script_counts = require "luaucdn".script_counts
local make_counter = require "counter"
local counters = setmetatable({}, {
	__index = function (self, key)
		local value = make_counter()
		self[key] = value
		return value
	end
})
local eprint = require "utils".eprint

local etymology_to_regular_language = {}
local get_non_etymological = require "Module:languages".getNonEtymological
local get_etymology_language_by_code = require "Module:etymology languages".getByCode
for code, data in pairs(require "Module:etymology languages/data") do
	etymology_to_regular_language[code] = get_non_etymological(get_etymology_language_by_code(code)):getCode()
end

local iter = require 'iterate_templates'
xpcall(function ()
	for _, template_name in ipairs(template_names) do
		for link, title, template in iter.iterate_links_raw(iter.iterate_cbor_templates(assert(io.open("cbor/" .. template_name .. ".cbor", "rb")):read "a")) do
			local lang = link.lang
			lang = lang and lang:trim()
			lang = etymology_to_regular_language[lang] or lang
			if lang and lang ~= "" and link.term and link.term ~= "" then
				local script_set = get_script_counts(link.term)
				script_set:sort()
				local script_key = table.concat(script_set, ", ")
				counters[lang]:count(script_key)
				
				count = count + 1
				if count >= limit then
					break
				end
			end
		end
	end
end, function (err)
	io.stderr:write(tostring(err), "\n", debug.traceback(), "\n")
end)

for lang, counter in require "mediawiki.table".sortedPairs(counters) do
	local value = { script_combinations = counter, lang = lang }
	print(require "json".encode(value))
end