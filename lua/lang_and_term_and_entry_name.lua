#! /usr/bin/env lua53

local iterate_templates = require "iterate_templates"

require "mediawiki"
local languages = require "Module:languages"
local etymology_languages = require "Module:etymology languages"
local families = require "Module:families"
local memoize = require "Module:fun".memoize
local get_language = memoize(languages.getByCode)
local get_etymology_language = memoize(etymology_languages.getByCode)
local get_family = memoize(families.getByCode)
languages.getByCode = get_language
etymology_languages.getByCode = get_etymology_language
families.getByCode = get_family

local function get_language_object(language_code)
	local lang = get_language(language_code)
		or get_etymology_language(language_code)
		or get_family(language_code)
	
	if lang then
		return languages.getNonEtymological(lang)
	end
end
local get_Unicode_script_set, get_Wiktionary_script_set =
	require "script".get_Unicode_script_set,
	require "script".get_Wiktionary_script_set

local iterate_links_raw = iterate_templates.iterate_links_raw
local iterate_cbor_templates = iterate_templates.iterate_cbor_templates

local count = 0
local limit = math.maxinteger
local args = { ... }
local total_processed = 0
local total_links = 0
local json_encode = require "cjson".encode
xpcall(function ()
	for _, template_name in ipairs(args) do
		local filename = ("cbor/%s.cbor"):format(template_name)
		for link, title, template in iterate_links_raw(iterate_cbor_templates(
				assert(io.open(filename, "rb")):read "a")) do
			local lang = link.lang
			local term = link.term
			total_links = total_links + 1
			if lang and term and term ~= "-" then
				local lang_obj = get_language_object(lang)
				if lang_obj and lang_obj.makeEntryName then
					print(json_encode {
						lang = lang_obj:getCode(),
						term = term,
						entry_name = lang_obj:makeEntryName(term)
					})
					count = count + 1
					if count >= limit then
						break
					end
				end
			end
		end
	end
end, function (err)
	io.stderr:write(debug.traceback(tostring(err), 2), "\n")
end)

io.stderr:write("total number of links: ", total_links, "\n")