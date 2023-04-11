require "mediawiki"
local languages = require "Module:languages"
local families = require "Module:families"
local etymology_languages = require "Module:etymology languages"
local function get_by_code(code)
	return languages.getByCode(code)
		or families.getByCode(code)
		or etymology_languages.getByCode(code)
end

local first_arg = ...
local max = type(first_arg) == "string" and tonumber(first_arg) or math.huge
local count = 0

local iterate_templates = require "iterate_templates"
local cbor = true
local function iter(str)
	return cbor
		and iterate_templates.iterate_links_raw(iterate_templates.iterate_cbor_templates(str))
		or iterate_templates.iterate_links(str)
end

local args = { ... }
xpcall(function()
	for _, file in ipairs(args) do
		local content = assert(assert(io.open(file, 'rb')):read 'a')
		for link, title, template in iter(content) do
			if link.lang then
				local language_obj = get_by_code(link.lang)
				local parent = language_obj and languages.getNonEtymological(language_obj)
				link.parent_code = parent and parent:getCode()
				
				if link.parent_code and link.term then
					process_link(link, title, template)
				end
				
				count = count + 1
				if count > max then
					break
				end
			end
		end
	end
end, function (err)
	io.stderr:write(debug.traceback(tostring(err), 2), "\n")
end)