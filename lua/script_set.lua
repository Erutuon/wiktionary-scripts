local M = {}

local ffi = require "ffi"
local ucdn = ffi.load "ucdn"

ffi.cdef "int ucdn_get_script(uint32_t code);"

---[[
local function make_script_name_to_number()
	local property_value_aliases = assert(io.open("PropertyValueAliases.txt", "rb")):read "a"
	local header = assert(io.open("ucdn.h", "rb")):read "a"
	local script_name_to_code = {}
	for code, name in property_value_aliases:gmatch "%f[^\n]sc%s*;%s*(%S+)%s*;%s*(%S+)" do
		script_name_to_code[name:upper()] = code:upper()
	end
	
	local name_to_number = {}
	for script, number in header:gmatch "#define UCDN_SCRIPT_(%S+)%s+(%d+)" do
		number = tonumber(number)
		name_to_number[script] = number
		name_to_number[script_name_to_code[script]] = number
	end
	return name_to_number
end
--]]

local mt = {}
local script_name_to_number = make_script_name_to_number()
function mt:__index(key)
	if type(key) == "string" then
		return self[script_name_to_number[key:upper()]]
	end
end

function M.script_set(str)
	local set = {}
	for _, code_point in utf8.codes(str) do
		local script_num = ucdn.ucdn_get_script(code_point)
		set[script_num] = (set[script_num] or 0) + 1
	end
	return setmetatable(set, mt)
end

return M