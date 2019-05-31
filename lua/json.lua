-- From Module:Json on English Wiktionary

local p = {}

require "mediawiki"

local escape_char_map = {
	["\\"] = "\\\\",
	["\""] = "\\\"",
	["\b"] = "\\b",
	["\f"] = "\\f",
	["\n"] = "\\n",
	["\r"] = "\\r",
	["\t"] = "\\t",
}

local function escape_char(c)
	return escape_char_map[c] or string.format("\\u%04x", c:byte())
end

-- This function makes an effort to convert an arbitrary Lua value to a string
-- containing a JSON representation of it. It's not intended to be very robust,
-- but may be useful for prototyping.
function p.encode(val, opts)
	opts = opts or {}
	function converter(val)
		local t = type(val)
		if t == 'nil' then
			return 'null'
		elseif t == 'boolean' then
			return val and 'true' or 'false'
		elseif t == 'number' then
			return p.numberFromNumber(val)
		elseif t == 'string' then
			return p.stringFromString(val)
		elseif t == 'table' then
			local key = next(val)
			if type(key) == 'number' then
				return p.arrayFromTable(val, converter)
			elseif type(key) == 'string' then
				return p.objectFromTable(val, converter)
			elseif type(key) == 'nil' then
				if opts.emptyTable == 'array' then
					return '[]'
				elseif opts.emptyTable == 'null' then
					return 'null'
				else
					return '{}'
				end
			else
				error('Table with unsupported key type: ' .. type(key))
			end
		else
			error('Unsupported type: ' .. t)
		end
	end
	return converter(val)
end

-- Given a string, escapes any illegal characters and wraps it in double-quotes.
-- Raises an error if the string is not valid UTF-8.
function p.stringFromString(s)
	if type(s) ~= 'string' or not mw.ustring.isutf8(s) then
		error('Not a valid UTF-8 string: ' .. s)
	end
	
	s = s:gsub('[%z\1-\31\\"]', escape_char)
	
	-- Not needed for valid JSON, but needed for compatibility with JavaScript:
	s = s:gsub('\226\128\168', '\\u2028')
	s = s:gsub('\226\128\169', '\\u2029')
	return '"' .. s .. '"'
end

-- Given a finite real number x, returns a string containing its JSON
-- representation, with enough precision that it *should* round-trip correctly
-- (depending on the well-behavedness of the system on the other end).
function p.numberFromNumber(x)
	if type(x) ~= 'number' then
		error('Not of type "number": ' .. x .. ' (' .. type(x) .. ')')
	end
	if x ~= x or x == math.huge or x == -math.huge then
		error('Not a finite real number: ' .. x)
	end
	return string.format("%.17g", x)
end

-- Given nil, returns the string 'null'. (Included for completeness' sake.)
function p.nullFromNil(v)
	if type(v) ~= 'nil' then
		error('Not nil: ' .. v .. ' (' .. type(v) .. ')')
	end
	return 'null'
end

-- Given true or false, returns the string 'true' or the string 'false'.
-- (Included for completeness' sake.)
function p.trueOrFalseFromBoolean(b)
	if type(b) ~= 'boolean' then
		error('Not a boolean: ' .. b .. ' (' .. type(b) .. ')')
	end
	return b and 'true' or 'false'
end

-- Given a table, treats it as an array and assembles its values in the form
-- '[ v1, v2, v3 ]'. Optionally takes a function to JSONify the values before
-- assembly; if that function is omitted, then the values should already be
-- strings containing valid JSON data.
function p.arrayFromTable(t, f)
	f = f or function (x) return x end
 
	local ret = {}
	for _, elem in ipairs(t) do
		elem = f(elem)
		if elem ~= nil then
			table.insert(ret, ', ')
			table.insert(ret, elem)
		end
	end
 
	if # ret == 0 then
		return '[]'
	end
 
	ret[1] = '[ '
	table.insert(ret, ' ]')
 
	return table.concat(ret)
end

-- Given a table whose keys are all strings, assembles its keys and values in
-- the form '{ "k1": v1, "k2": v2, "k3": v3 }'. Optionally takes a function to
-- JSONify the values before assembly; if that function is omitted, then the
-- values should already be strings containing valid JSON data. (The keys, by
-- contrast, should just be regular Lua strings; they will be passed to this
-- module's jsonStringFromString.)
function p.objectFromTable(t, f)
	f = f or function (x) return x end
 
	local ret = {}
	for key, value in require "Module:table".sortedPairs(t) do
		if type(key) ~= 'string' then
			error('Not a string: ' .. key)
		end
		key = p.stringFromString(key)
		value = f(value)
		if value ~= nil then
			table.insert(ret, ', ')
			table.insert(ret, key .. ': ' .. value)
		end
	end
 
	if # ret == 0 then
		return '{}'
	end
 
	ret[1] = '{ '
	table.insert(ret, ' }')
 
	return table.concat(ret)
end

return p
