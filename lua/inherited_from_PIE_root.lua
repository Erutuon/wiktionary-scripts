local function make_root_set()
	local template_dump = assert(assert(io.open 'templates/ine-root.txt'):read 'a')
	local root_set= {}
	for title in template_dump:gmatch '\1Reconstruction:.-/([^\n]+)' do
		root_set[title] = true
	end
	return root_set
end

local root_set = make_root_set()
local iterate_links = require 'iterate_templates'.iterate_links
local inherited_templates = assert(assert(io.open 'templates/inherited.txt'):read 'a')

require 'mediawiki'
local inherited_from_root = {}
setmetatable(inherited_from_root, {
	__index = function (self, k)
		local val = require 'Module:array'()
		self[k] = val
		return val
	end
})

for link, title, template in iterate_links(inherited_templates) do
	if link.lang == 'ine-pro' and link.term and root_set[link.term:match '^%*(.+)$']
	and not link.alt then
		inherited_from_root[title]:insert(template.text)
	end
end

local sorted_pairs = require 'Module:table'.sortedPairs
local case_insensitive_comp = require "casefold".comp

local cjson = require 'cjson'
for title, templates in sorted_pairs(inherited_from_root, case_insensitive_comp) do
	print(cjson.encode { title = title, templates = templates })
end