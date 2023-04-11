local M = {}

function M.iter(file)
	return coroutine.wrap(function ()
		file = file or io.stdin
		local decode = require "cjson".decode
		local yield = coroutine.yield
		for line in file:lines() do
			yield(decode(line))
		end
	end)
end

return M