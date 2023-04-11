local mt = {
	__index = {
		count = function (self, k)
			self[k] = (self[k] or 0) + 1
		end,
		print = function (self)
			for k, v in pairs(self) do
				print(k, v)
			end
		end
	}
}

function make_counter(t)
	local counter = t or {}
	return setmetatable(counter, mt)
end

return make_counter