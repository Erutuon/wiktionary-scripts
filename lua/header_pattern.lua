require "insert_lpeg"

local ws = S " \n"

local header_pattern = P {
	(V "header" + (V "comment" + V "nowiki" + (1 - V "header"))^0 * V "header") * Cp(),
	header = (B "\n" + Cmt(true, function (_, pos) return pos == 1 end))
		* V "comment"^0
		* (((C(P "="^1)
			* Cs((P "="^0 * (((1 - (S "\n=" + P "<!--"))^1 + V "comment" / "")^1 - V "after_header"))^0)
			* C(P "="^1))
			/ function (start_equals, inner, end_equals)
					-- print("start_equals", start_equals, "inner", inner, "end_equals", end_equals)
					local header_level = math.min(#start_equals, #end_equals)
					local header_content = inner
					
					if header_level < #end_equals then
						header_content = header_content .. ("="):rep(#end_equals - header_level)
					elseif header_level < #start_equals then
						header_content = ("="):rep(#start_equals - header_level) .. header_content
					end
					
					-- if #inner == 0 then if #start_equals > 1 and #end_equals > 1 then
					
					return header_level, header_content
				end
		+ C(P "="^3)
			/ function (equals)
				local header_level = #equals // 2
				local header_content = ("="):rep(#equals % 2 == 0 and 2 or 1)
				return header_level, header_content
			end)
		* V "after_header"),
		--[[
		* Cmt(C(P "="^1) * C(P "="^0 * (V "comment" + (1 - (P "=" + P "<!--")))^1) * C(P "="^1, "last_equals"),
			function (_, _, equals1, inner, equals2)
				print("equals1", equals1, "inner", inner, "equals2", equals2)
				local header_level = math.min(#equals1, #equals2)
				local header_content = inner:sub(1, -#equals2)
				return true, header_level, header_content
			end),
		--]]
	after_header = (S " \t"^1 + V "comment")^0 * #(P "\n" + P(-1)),
	comment = P "<!--" * (1 - P "-->")^0 * P "-->",
	nowiki = P "<nowiki" * (ws^0 * (P ">" * (1 - V "close_nowiki")^0 * V "close_nowiki" + P "/>")),
	close_nowiki = "</nowiki" * ws^0 * ">",
}

if ... == "test" then
	local failures = 0
	for _, example in ipairs {
		{ "==plain==", 2, "plain"},
		{ "===equals in header === ===", 3, "equals in header === " },
		{ "===", 1, "=" },
		{ "====", 2, "==" },
		{ "==comment in header<!-- -->==", 2, "comment in header" },
		{ "==comment after header==<!--\n\n-->  ", 2, "comment after header" },
		{ "<!--\n==header in comment\n-->", nil, nil },
		{ "<!--\n-->==header preceded by comment==", 2, "header preceded by comment" },
		{ "<nowiki>==header in nowiki==</nowiki>", nil, nil },
		{ "<nowiki />==header after nowiki==", nil, nil },
		{ "<nowiki>  </nowiki>==header after nowiki==", nil, nil },
	} do
		local header, expected_level, expected_content = table.unpack(example)
		local level, content = header_pattern:match(header)
		if not (level == expected_level and content == expected_content) then
			failures = failures + 1
			
			print(header)
			print(("expected: %q, %q"):format(expected_level, expected_content))
			print(("actual: %q, %q"):format(level, content))
		end
	end
	
	if failures == 0 then
		print("Success!")
	end
	
	return
end

return header_pattern