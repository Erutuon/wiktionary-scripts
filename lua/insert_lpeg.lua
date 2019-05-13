local lpeg = require 'lpeg'

for k, v in pairs(lpeg) do
	if type(k) == 'string' and k:find '^%u' then
		_ENV[k] = v
	end
end