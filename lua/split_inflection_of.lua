local max = math.huge

local iterate_templates = require "iterate_templates".iterate_templates
local Array = require "mediawiki.array"

local titles_mt = {}
function titles_mt:__index(k)
	local val = Array()
	self[k] = val
	return val
end

local langs = {}
local langs_mt = {}

function langs_mt:__gc()
	for lang, data in pairs(self) do
		local file = assert(io.open(lang .. ".txt", "wb"))
		for title, templates in pairs(data) do
			file:write("\1", title)
			for _, template in ipairs(templates) do
				file:write("\n", template)
			end
		end
		file:close()
	end
end

assert(type(langs_mt.__gc) == "function")

setmetatable(langs, langs_mt)

function langs_mt:__index(k)
	local val = setmetatable({}, titles_mt)
	self[k] = val
	return val
end

function string:trim()
	return self:match "^%s*(.-)%s*$"
end

local count = 0
for template, title in iterate_templates(assert(io.read "a")) do
	local lang = template.parameters.lang or template.parameters[1]
	lang = lang and lang:trim()
	if lang then
		langs[lang][title]:insert(template.text)
	end
	
	count = count + 1
	if count > max then
		break
	end
end