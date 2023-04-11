#! /usr/bin/env lua53

local iterate_templates = require "iterate_templates"

require "mediawiki"
local memoize = require "Module:fun".memoize
local get_language_object = memoize(require "Module:languages".getByCode)
local get_Unicode_script_set, get_Wiktionary_script_set =
	require "script".get_Unicode_script_set,
	require "script".get_Wiktionary_script_set

local data = require "data_by_title" "templates"

local iterate_links_raw = iterate_templates.iterate_links_raw
local iterate_cbor_templates = iterate_templates.iterate_cbor_templates

local Array = require "Module:array"
local function script_set_string(script_set)
	return Array.keys(script_set)
		:sort()
		:concat ", "
end

local exclusions = Array {
	"None", "Zyyy", "Zinh", "Zmth", "Zsym",
}:to_set()
local replacements = {
	Geok = "Geor", Latinx = "Latn", ["as-Beng"] = "Beng", polytonic = "Grek", Cyrs = "Cyrl",
}
local function transform_script_code(code)
	if exclusions[code] then
		return nil
	end
	return replacements[code] or code
end
transform_script_code = nil

local function get_Wiktionary_script_set_string(s)
	return script_set_string(get_Wiktionary_script_set(
		s,
		transform_script_code))
end

local count = 0
local limit = math.maxinteger
local args = { ... }
local total_processed = 0
local total_links = 0
xpcall(function ()
	for obj in require "jsonl".iter() do
		local lang, term, entry_name = obj.lang, obj.term, obj.entry_name
		if not (lang and term and entry_name) then
			error("invalid JSON object in line " .. total_links)
		end
		total_processed = total_processed + 1
		local scripts1, scripts2, scripts3 =
			script_set_string(get_Unicode_script_set(term, transform_script_code)),
			get_Wiktionary_script_set_string(term),
			get_Wiktionary_script_set_string(entry_name)
		
		if not (scripts1 == scripts2 and scripts2 == scripts3) then
			print(term:gsub("[\t\n\r]", " "), lang, scripts1, scripts2, scripts3)
			
			count = count + 1
			if count >= limit then
				break
			end
		end
	end
end, function (err)
	io.stderr:write(debug.traceback(tostring(err), 2), "\n")
end)

io.stderr:write("total number of links whose script was analyzed: ", total_processed, "\n")
io.stderr:write("total number of links with differing script sets: ", count, "\n")