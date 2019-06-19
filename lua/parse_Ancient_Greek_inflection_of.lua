#! /usr/bin/env lua

local iterate_templates = require 'iterate_templates'.iterate_templates
local form_of_data = require 'mediawiki.form_of_data'
local iter = require 'windows.iter'
local Array = require 'mediawiki.array'
require 'insert_lpeg'

-- utils also provides string.trim
local eprint = require 'utils'.eprint

local first_tag_group_pattern = C((1 - P '//')^1)

-- For now, return tag type for first of several conjoined tags.
local function get_tag_type(tag)
	tag = first_tag_group_pattern:match(tag)
	if tag then
		tag = form_of_data.shortcuts[tag] or tag
		local data = form_of_data.tags[tag]
		if data then
			return data.tag_type
		end
	end
end

local function iter_tag_types(tags)
	tags = first_tag_group_pattern:match(tags)
	
	return coroutine.wrap(function ()
		if tags then
			local pos = 1
			while true do
				local tag
				tag, pos = tags:match('([^:]+)()', pos)
				
				if not tag then
					break
				end
				
				tag = form_of_data.shortcuts[tag] or tag
				local data = form_of_data.tags[tag]
				coroutine.yield(data and data.tag_type)
			end
		end
	end)
end

function string:strip_comments()
	return (self:gsub('<!%-%-.-%-%->', ''))
end

local counts = setmetatable({}, {
	__index = {
		add = function (self, tags, template)
			local key = tags:concat(', ')
			if key and key ~= '' then
				self[key] = self[key] or Array()
				self[key]:insert(template)
			end
		end
	},
})

local count = 0
local max = math.huge

for template, title in iterate_templates(assert(io.read 'a')) do
	local parameters = template.parameters
	local lang = parameters[1] or parameters.lang
	lang = lang and lang:trim()
	if lang == 'grc' then
		-- print(template.text)
		template.title = title
		
		local tags = Array()
		local i = parameters.lang and 3 or 4
		while true do
			local param = parameters[i]
			if not param then
				break
			end
			
			param = param:trim():strip_comments()
			
			if (param == 'and' or param == 'or') and get_tag_type(parameters[i + 1]) == get_tag_type(parameters[i - 1]) then
				i = i + 2 -- Skip 'and' or 'or' and the parameter after it.
			else
				i = i + 1
				
				if param == ';' then -- Start new tags table
					counts:add(tags, { text = template.text, title = template.title })
					tags = Array()
				else
					for tag_type in iter_tag_types(param) do
						tags:insert(tag_type or param == '' and 'empty' or 'unknown')
					end
				end
			end
		end
		
		counts:add(tags, { text = template.text, title = template.title })
		
		count = count + 1
		if count > max then
			break
		end
	end
end

setmetatable(counts, nil)

-- print(require 'inspect'(counts))

local counts_sorted = Array()

local summary = Array()

local utf8proc = require 'lutf8proc'
local function make_Greek_sortkey(word)
	return utf8proc.map(word, 'decompose', 'casefold')
end

print '__NOTOC__\n{| class="wikitable sortable"\n! tags !! template count'

--[[
counts = require 'Module:fun'.filter(
	function (_, tags)
		local prev
		for tag in tags:gmatch ' ?([^,]+)' do
			if tag == prev and not (tag == 'empty' or tag == 'unknown') then
				return true
			end
			
			prev = tag
		end
		return false
	end,
	counts)
--]]

local function format_tag(tag_string)
	return '[[#' .. tag_string .. '|' .. tag_string .. ']]'
end

for tag_string, templates in require 'Module:table'.sortedPairs(counts) do
	local count = #templates
	-- counts_sorted:insert { tag_string, count }
	print('|-\n| ' .. format_tag(tag_string) .. ' || ' .. #templates)
	
	--[=[
	summary:insert('\n==' .. tag_string .. '==')
	summary:insert(templates
		:sort(
			function (template1, template2)
				return make_Greek_sortkey(template1.title)
					< make_Greek_sortkey(template2.title)
			end)
		:fold(
			function (new_t, template)
				local prev = new_t[#new_t]
				if prev and prev[1] and prev[1].title == template.title then
					prev:insert(template)
				else
					new_t:insert(Array { template })
				end
				return new_t
			end,
			Array())
		:map(
			function (templates)
				return '; [[' .. templates[1].title .. ']]\n'
					.. templates:map(
						function (template)
							return '<pre><nowiki>' .. template.text .. '</nowiki></pre>'
						end)
						:concat '\n'
			end)
		:concat '\n')
	--]=]
end

print '|}'

print(summary:concat '\n')

--[[
table.sort(counts_sorted, function (count1, count2)
	return count1[2] > count2[2]
end)
--]]

--[[
io.stderr:write '{| class="wikitable"\n'
for i, count in ipairs(counts_sorted) do
	if i > 1 then
		io.stderr:write('|-\n')
	end
	io.stderr:write('| ', count[1]:gsub('_', ' '), ' || ', count[2], '\n')
end
io.stderr:write '|}\n'
--]]