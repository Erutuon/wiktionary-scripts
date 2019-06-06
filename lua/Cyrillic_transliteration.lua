local iter = require "iterate_templates"

local count = 0
local max = math.huge

local data_by_title = require "data_by_title"("templates")

local find = require "lua-utf8".find
local decompose = require "lutf8proc".decomp

for link, title, template in iter.iterate_links(assert(io.read 'a')) do
	if link.lang == "bg" and link.tr and find(decompose(link.tr), "[\u{0300}\u{0301}]") then
		data_by_title[title]:insert {
			template = template.text,
			term = link.term,
			term_param = link.term_param,
			tr = link.tr,
			tr_param = link.tr_param,
		}
		
		count = count + 1
		if count > max then
			break
		end
	end
end