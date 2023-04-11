#! /usr/bin/env lua53

local title_data = {}

local cbor_file = cbor_file_path and io.open(cbor_file_path, "rb") or io.stdin
local cbor = assert(cbor_file:read 'a')

xpcall(function ()
	for template, title in require "iterate_templates"(cbor) do
		local lang = template.parameters[1]
		if lang == "da" or lang == "nb" or lang == "no" or lang == "sv" then
			title_data[title] = title_data[title] or {}
			title_data[title][lang] = true
		end
	end
end, function (err)
	io.stderr:write(debug.traceback(tostring(err), 2), "\n")
end)

local function_to_file = {
	[function(langs)
		return not langs.da and langs.nb
	end] = io.open("nb_without_da.txt", "wb"),
	[function(langs)
		return not langs.sv and (langs.da or langs.nb or langs.no)
	end] = io.open("da_nb_no_without_sv.txt", "wb"),
}
for title, langs in require "mediawiki.table".sortedPairs(title_data, require "casefold".comp) do
	for func, file in pairs(function_to_file) do
		if func(langs) then
			file:write(title, "\n")
		end
	end
end

for _, file in pairs(function_to_file) do
	file:close()
end