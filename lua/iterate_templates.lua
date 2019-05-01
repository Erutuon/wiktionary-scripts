local parse_template = require "parse_template"

return function (content, title_start, template_start)
	title_start = title_start or '\1'
	template_start = template_start or '\n'
	local pattern = title_start .. '(.-)' .. template_start .. '(.-)%f[' .. title_start .. '\0]'
	
	local function yielder ()
		for title, text in content:gmatch(pattern) do
			for pos in text:gmatch(template_start .. '(){{') do
				local success, template = pcall(parse_template, text, pos)
				
				if success then
					coroutine.yield(title, template)
				end
			end
		end
	end
	
	local co = coroutine.create(yielder)
	
	return function ()
		return select(2, coroutine.resume(co))
	end
end