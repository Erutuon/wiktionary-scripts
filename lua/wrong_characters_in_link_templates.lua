#! /usr/bin/env lua53

require "mediawiki"

local json = require "cjson"
local parse = require "parse_template"
local eprint = require "utils".eprint
local chars_from_names = require "utils".chars_from_names

local languages = require "Module:languages"
local families = require "Module:families"
local etymology_languages = require "Module:etymology languages"
local function get_by_code(code)
	return languages.getByCode(code)
		or families.getByCode(code)
		or etymology_languages.getByCode(code)
end

local max = type((...)) == "string" and tonumber(...) or math.huge
local count = 0

local link_template_names = require "Module:table".listToSet {
	"m", "l", "t", "t+", "cog", "cognate", "ncog", "noncognate"
}

local derivation_template_names = require "Module:table".listToSet {
	"der", "derived", "inh", "inherited", "bor", "borrowed",
	"cal", "calq", "calque", "clq",
}

local Array = require "Module:array"
local function make_script_pattern(...)
	return Array(...)
		:map(function (script) return "\\p{" .. script .. "}" end)
		:concat ''
end


local function Arabic_letters(...)
	return chars_from_names(Array(...)
		:map(function (name) return "Arabic letter " .. name end))
end

local non_Persian = Arabic_letters(
	"kaf", "yeh", "alef maksura", "teh marbuta",
	"heh doachashmee", "heh goal")
local non_Pashto = Arabic_letters("kaf", "alef maksura", "teh marbuta")
local Pashto_only_final_yeh = Arabic_letters("Farsi yeh", "yeh with tail")
local non_Urdu = Arabic_letters("kaf", "yeh", "alef maksura", "teh marbuta", "heh")
local non_Ottoman = Arabic_letters(
	"keheh", "yeh", "alef maksura", "teh marbuta", "heh doachashmee", "heh goal")
local Pashto_yeh_in_wrong_position = "[" .. Pashto_only_final_yeh .. "]" .. "\\B"
local all_Arabic = make_script_pattern("Arab", "Zinh", "Zyyy")
local Greek_symbols = chars_from_names(
	"Greek theta symbol", "Greek phi symbol", "Greek kappa symbol", "Greek rho symbol")

local rure = require "luarure"

local data_by_title = require "data_by_title"
local function make_data(file)
	return data_by_title("templates", file)
end

local combined_Arabic_data = make_data(assert(io.open("Arabic.json", "wb")))

local language_data = {
	ar = {
		regex = rure.new("["
			.. Arabic_letters("Farsi yeh", "heh doachashmee", "heh goal", "keheh", "yeh with tail")
			.. "[^" .. make_script_pattern("Arab", "Brai", "Zinh", "Zyyy") .. "]]"),
		title_to_data = make_data(assert(io.open("ar.json", "wb")))
	},
	en = {
		regex = rure.new("[^" .. make_script_pattern("Latn", "Brai", "Zinh", "Zyyy") .. "]"),
		title_to_data = make_data(assert(io.open("en.json", "wb"))),
	},
	el = {
		regex = rure.new("[" .. Greek_symbols .. "[^" .. make_script_pattern("Grek", "Zinh", "Zyyy") .. "]]"),
		title_to_data = make_data(assert(io.open("el.json", "wb"))),
	},
	grc = {
		regex = rure.new("[" .. Greek_symbols .. chars_from_names("Greek koronis", "Greek psili")
			.. "[^" .. make_script_pattern("Grek", "Cprt", "Zinh", "Zyyy") .. "]]"),
		title_to_data = make_data(assert(io.open("grc.json", "wb"))),
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
		regex = rure.new("[" .. non_Persian .. "[^" .. make_script_pattern("Latn", "Arab", "Zinh", "Zyyy") .. "]]"),
		title_to_data = combined_Arabic_data,
	},
	ota = {
		regex = rure.new("[" .. non_Ottoman .. "[^" .. all_Arabic .. "]]"),
		title_to_data = combined_Arabic_data,
	},
}

local Cyrillic_data = {
	regex = rure.new("[^" .. make_script_pattern("Cyrl", "Zinh", "Zyyy") .. "]"),
	title_to_data = make_data(assert(io.open("Cyrillic.json", "wb"))),
}

local Cyrillic_only_languages = Array()
for code, data in pairs(require "Module:languages/alldata") do
	local scripts = Array(data.scripts)
	if scripts:contains "Cyrl"
			and (#scripts == 1
			or #scripts == 2 and scripts:contains "Brai") then
		Cyrillic_only_languages:insert(code)
	end
end

for _, lang in ipairs(Cyrillic_only_languages) do
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
		count = count + 1
		
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