local M = {}

local Array = require "mediawiki.array"

local uniname = require "uniname"
function M.chars_from_names(...)
	return Array(...)
		:map(function (name) return utf8.char(uniname.to_codepoint(name)) end)
		:concat ''
end

function M.eprint(...)
	local vals = table.pack(...)
	for i = 1, vals.n do
		vals[i] = tostring(vals[i])
	end
	io.stderr:write(table.concat(vals, '\t'), '\n')
end

function string:trim()
	return self:gsub('^%s*(.-)%s*$', '%1')
end

local re_compile = require 're'.compile
function M.split_at_re(sep)
	local t = type(sep)
	if not (t == 'string' or t == 'number') then
		error(('bad argument #1 to split_at_re (string expected, got %s)'):format(t))
	else
		sep = tostring(sep)
	end
	
	local splitter = re_compile([[
		splitter      <- not_separator (%separator not_separator)*
		not_separator <- { (! %separator .)* }
	]], { separator = re_compile(sep) })
	
	return function (...)
		return splitter:match(...)
	end
end

return M