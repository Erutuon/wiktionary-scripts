#! /usr/bin/env lua53

require "mediawiki"

local json = require "cjson"
local parse = require "parse_template"

local languages = require "Module:languages"
local families = require "Module:families"
local etymology_languages = require "Module:etymology languages"
local function get_by_code(code)
	return languages.getByCode(code)
		or families.getByCode(code)
		or etymology_languages.getByCode(code)
end

local max = 10
local count = 0

local link_template_names = require "Module:table".listToSet {
	"m", "l", "t", "t+", "cog", "cognate", "ncog", "noncognate"
}

local derivation_template_names = require "Module:table".listToSet {
	"der", "derived", "inh", "inherited", "bor", "borrowed",
	"cal", "calq", "calque", "clq",
}

local function eprint(...)
	for i = 1, select("#", ...) do
		if i > 1 then
			io.stderr:write "\t"
		end
		io.stderr:write(tostring(select(i, ...)))
	end
	count = count + 1
	io.stderr:write "\n"
end

local function make_script_pattern(scripts)
	return require 'Module:array'(scripts)
		:map(function (script) return "\\p{" .. script .. "}" end)
		:concat ''
end

-- Arabic letter kaf, Arabic letter yeh, Arabic letter alef maksura, Arabic letter teh marbuta
local non_Persian = "كيىة"
-- Arabic letter kaf, Arabic letter alef maksura, Arabic letter teh marbuta
local non_Pashto = "كىة"
-- Arabic letter Farsi yeh, Arabic letter yeh with tail
local Pashto_only_final_yeh = "یۍ"
-- Arabic letter heh
local non_Urdu = non_Persian .. "ه"
local Pashto_yeh_in_wrong_position = "[" .. Pashto_only_final_yeh .. "]" .. "\\B"
local all_Arabic = make_script_pattern { "Arab", "Zinh", "Zyyy" }

local rure = require "luarure"

local data_by_title = require "data_by_title"
local function make_data(file)
	return data_by_title("templates", file)
end

local combined_Arabic_data = make_data(assert(io.open("Arabic.txt", "wb")))

local language_data = {
	ar = {
		-- Arabic letter keheh, Arabic letter Farsi yeh,
		-- Arabic letter yeh with tail, Arabic letter Farsi yeh,
		-- Arabic letter heh goal, Arabic letter heh doachashmee
		regex = rure.new("[" .. "کیۍیہھ"
			.. "[^" .. make_script_pattern { "Arab", "Brai", "Zinh", "Zyyy" } .. "]]"),
		title_to_data = make_data(assert(io.open("ar.txt", "wb")))
	},
	en = {
		regex = rure.new("[^" .. make_script_pattern { "Latn", "Brai", "Zinh", "Zyyy" } .. "]"),
		title_to_data = make_data(assert(io.open("en.txt", "wb"))),
	},
	el = {
		regex = rure.new("[^" .. make_script_pattern { "Grek", "Zinh", "Zyyy" } .. "]"),
		title_to_data = make_data(assert(io.open("el.txt", "wb"))),
	},
	grc = {
		regex = rure.new("[^" .. make_script_pattern { "Grek", "Cprt", "Zinh", "Zyyy" } .. "]"),
		title_to_data = make_data(assert(io.open("grc.txt", "wb"))),
	},
	fa = {
		regex = rure.new("[" .. non_Persian .. "[^" .. all_Arabic .. "]]"),
		title_to_data = combined_Arabic_data,
	},
	ps = {
		regex = rure.new("[" .. non_Pashto .. "[^" .. all_Arabic .. "]]|"
			.. Pashto_yeh_in_wrong_position),
		title_to_data = combined_Arabic_data,
	},
	ur = {
		regex = rure.new("[" .. non_Urdu .. "[^" .. all_Arabic .. "]]"),
		title_to_data = combined_Arabic_data,
	},
	sdh = {
		regex = rure.new("[" .. non_Persian .. "[^" .. make_script_pattern { "Latn", "Arab", "Zinh", "Zyyy" } .. "]]"),
		title_to_data = make_data(assert(io.open("sdh.txt", "wb"))),
	}
}

local Cyrillic_data = {
	regex = rure.new("[^" .. make_script_pattern { "Cyrl", "Zinh", "Zyyy" } .. "]"),
	title_to_data = make_data(assert(io.open("Cyrillic.txt", "wb"))),
}
for _, lang in ipairs {
	"abq", "ady", "agx", "akv", "alr", "alt", "ani", "aqc", "atv", "av", "ba",
	"bdk", "be", "bg", "bph", "bua", "ce", "chm", "cji", "cjs", "ckt",
	"crp-tpr", "cv", "dar", "ddo", "dlg", "ess", "evn", "gdo", "gin", "gld",
	"huz", "inh", "kap", "kbd", "kca", "ket", "khv", "kim", "kjh", "kjj",
	"kpt", "kpv", "kpy", "krc", "kum", "lbe", "lez", "mdf", "mk", "mns", "mrj",
	"mtm", "myv", "neg", "nio", "niv", "ru", "rue", "rut", "sah", "sel", "sjd",
	"sty", "syd-fne", "tab", "tkr", "tyv", "ude", "udm", "uk", "uum", "xal",
	"xas", "xme-klt", "ykg", "yrk", "ysr", "yux"
} do
	language_data[lang] = Cyrillic_data
end

for lang, data in pairs(language_data) do
	if type(data.regex.is_match) ~= "function" then
		print(lang, require 'inspect'(data))
	end
end

local uniname = require "uniname"
local function character_name(character)
	return uniname.from_codepoint(utf8.codepoint(character))
end

local function get_characters(str, character_regex)
	local matches = {}
	for match in character_regex:iter(str) do
		table.insert(matches, match)
	end
	return table.concat(matches)
end

local function process_link(link, title, template)
	local target = link.term
	if not target and target ~= "" then
		return
	end
	
	local data = language_data[link.parent_code]
	
	if data and data.regex:is_match(target) then
		data.title_to_data[title]:insert {
			template = template.text,
			text = target,
			param = link.term_param,
			chars = get_characters(target, data.regex),
			lang = link.parent_code,
		}
	end
end

local iterate_templates = require "iterate_templates"

for link, title, template in iterate_templates.iterate_links(io.read 'a') do
	if link.lang then
		local language_obj = get_by_code(link.lang)
		local parent = language_obj and languages.getNonEtymological(language_obj)
		link.parent_code = parent and parent:getCode()
		
		if link.parent_code and link.term then
			process_link(link, title, template)
		end
		
		if count > max then
			break
		end
	end
end