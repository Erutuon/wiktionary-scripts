for link, title, template in require 'iterate_templates'.iterate_links(io.read 'a') do
	for param, value in pairs(link) do
		if link[param .. '_param'] then
			local expected = template.parameters[link[param .. '_param']]
			if value ~= expected then
				print(param, value, expected)
			end
		end
	end
end