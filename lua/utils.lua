local M = {}

local Array = require "mediawiki.array"

local uniname = require "uniname"
function M.chars_from_names(...)
	return Array(...)
		:map(function (name) return utf8.char(uniname.to_codepoint(name)) end)
		:concat ''
end

function M.eprint(...)
	io.stderr:write(table.concat({...}, '\t'), '\n')
end

function string:trim()
	return self:gsub('^%s*(.-)%s*$', '%1')
end

return M