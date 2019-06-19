local data_by_title = require "data_by_title" "templates"
local iterate_templates = require "iterate_templates"
local rure = require "luarure"
local Array = require "mediawiki.array"

local chars_from_names = require "utils".chars_from_names

local ZWNJ = chars_from_names("zero width non-joiner")

local iter_prev_char
do
	local max_UTF8_bytes = 4
	function iter_prev_char(str, pos)
		return coroutine.wrap(function ()
			pos = pos - 1
			while pos >= 1 do
				local char = str:sub(math.max(pos - max_UTF8_bytes, 0), pos):match(utf8.charpattern .. "$")
				if char == "" then
					break
				end
				pos = pos - #char + 1
				coroutine.yield(char)
			end
		end)
	end
end

local function iter_next_char(str, pos)
	return coroutine.wrap(function ()
		while true do
			local char
			char, pos = str:match("^(" .. utf8.charpattern .. ")()", pos)
			if not char then
				break
			end
			coroutine.yield(char)
		end
	end)
end

local next_bad_ZWNJ
do
	-- Rules based on https://www.unicode.org/reports/tr31/tr31-31.html#A1
	
	-- Regex generated by https://unicode.org/cldr/utility/list-unicodeset.jsp
	-- with C-style escapes converted to Rust-style escapes.
	-- [[\p{Joining_Type=Dual_Joining}\p{Joining_Type=Left_Joining}]&\p{Script=Arabic}]
	local Arabic_left_joining = rure.new "[ئࢨࢩٮبٻپڀݐ-ݕࢠݖࢡࢶࢷتثٹٺټٽٿࢸجڃڄچڿڇࢢحخځڂڅݗݘݮݯݲݼسشښ-ڜۺݜݭݰݽݾصضڝࢯڞۻطظڟࢣعغڠۼݝ-ݟࢳفڡڢࢻڣڤࢤڥڦݠݡٯقڧࢼڨࢥكک-ڬݿڭڮࢴگࢰڰ-ڴݢػؼݣݤلڵ-ڸݪࢦمݥݦࢧنںࢽڻ-ڽڹݧ-ݩهھہۂۿىيٸیێېۑؽ-ؿؠݵ-ݷࢺݺݻ]"
	
	-- [[\p{Joining_Type=Dual_Joining}\p{Joining_Type=Right_Joining}]&\p{Script=Arabic}]
	local Arabic_right_joining = rure.new "[آأٲٱؤإٳݳݴئࢨࢩࢬاٵٮبٻپڀݐ-ݕࢠݖࢡࢶࢷة-ثٹٺټٽٿࢸجڃڄچڿڇࢢحخځڂڅݗݘݮݯݲݼدذڈ-ڍࢮڎ-ڐۮݙݚرزڑ-ڙۯݛݫݬݱࢪࢲࢹسشښ-ڜۺݜݭݰݽݾصضڝࢯڞۻطظڟࢣعغڠۼݝ-ݟࢳفڡڢࢻڣڤࢤڥڦݠݡٯقڧࢼڨࢥكک-ڬݿڭڮࢴگࢰڰ-ڴݢػؼݣݤلڵ-ڸݪࢦمݥݦࢧنںࢽڻ-ڽڹݧ-ݩهھہ-ۃۿەۀوٶۄ-ۇٷۈ-ۋࢱۏݸݹࢫىيٸی-ێېۑؽ-ؿؠݵ-ݷࢺےۓݺݻ]"
	
	-- \p{Joining_Type=Transparent}
	local transparent = rure.new "[\u{00AD}\u{034F}҈҉֑-ֽׅ֯ׄؐ-ؚ\u{061C}ۖ-ۜ۟-۪ۤۧۨ-ۭ\u{070F}݄݀݃݇-࣓݊-࣡࣪-࣯॒༘༙༵༷࿆ࣳ॑ྂྃ྆྇\u{17B4}\u{17B5}៓\u{180B}-\u{180D}᩿᭫-᭳᳐-᳔᳒-᳢᳠-᳨᳴᳸᳹\u{200B}\u{200E}\u{200F}\u{202A}-\u{202E}\u{2060}-\u{2064}\u{206A}-\u{206F}⵿꙰-꙲꣠-꣱\u{FE00}-️︡︣-︨︪︦-︭︯\u{FEFF}\u{FFF9}-\u{FFFB}𐋠𑍦-𑍬𑍰-𑍴\u{13430}-\u{13438}\u{1BCA0}-\u{1BCA3}𝅧-𝅩\u{1D173}-𝆂𝆅-𝆋𝆪-𝆭𝉂-𝉄𝨀-𝨶𝨻-𝩬𝩵𝪄𝪛-𝪟𝪡-𝪯𞣐-𞣖\u{E0001}\u{E0020}-\u{E007F}\u{E0100}-\u{E01EF}̸̧̨̲̓̓҆⳱̔҅⳰́́॔̀̀॓̆̂̌̊͂̈̈́̋̃̇̄̍̎̒̽̕̚-̿͆͊-͌͐-͒͗͛҄҇݁݅͝͞់-៑៝᪰-᪴᪻᪼᷀᷁᷃-᷉᷋-᷎᷑᷵-᷸᷻᷾⃰⳯꙼꙽𐫥𐴤-𐴧𐽈-𐽊𐽌𛲝̖-̙̜-̠̩-̬̯̳̺-̼͇-͉͍͎͓-͖͙͚݂݆߽࡙͜͟͢-࡛᪵-᷐᪺᪽᷂᷏᷹᷽᷿⃬᷼-⃯︧𐨍𐫦𐽆𐽇𐽋𐽍-̶̷⃘𐽐-⃚⃥⃪⃫𛲞᪾⃝-⃠⃢-⃤̵゙゚̅̉̏-̡̛̑-̴ְ̦̭̮̰̱̹︩︢︠҃︮꙯͘͠͡ͅ-ָׇֹ-ֻּֿׁׂﬞࠜ-ࠣࠥ-ࠧࠩ-ًࣰٌࣱٍࣲَُِّࣩࣦࣶࠬ࠘࠙࠭ࣧࣨࣤࣴࣵࣥࣾૻ𑈷ْૺ𑈾ٓૼٕٟٖٔ-٘ࣿٙ-ٰܑࣣࣹࣺٞࣷࣸࣽࣻࣼܰ-ܿ߫-𖫰߳፟፞፝꛰꛱-𖫴𞥄-𞥊𞥆𞥇-़়਼઼𞥉૽-૿଼಼᬴᯦᰷꦳𑂺𑅳𑇊𑈶𑋩𑌻𑌼𑑆𑓃𑗀𑚷𑠺𑨳𑵂ऀँঁਁઁଁఀಁഁᬀᬁꣅꦀ𑂀𑄀𑆀𑌁𑑃𑒿𑖼𑙀𑨵-𑨷𑰼𑲶𑵃ंਂંஂఄഀཾံំᩴᬂᮀ᳭ꠋꦁ𐨎𑀁𑂁𑄁𑆁𑈴𑋟𑌀𑑄𑓀𑖽𑘽𑚫𑠷𑨸𑪖𑰽𑲵𑵀𑶕𐨏𑄂𑵁৾𑇉𑑞ੰੱᬃꦂᮁ𐨸-𐨺𑇋𑇌𑪘๎็-ํ່-ໍ༹꤫꪿꫁-့꤭៉៊᩵-᤹᩼-᤻𖬰𞄱𖬱𞄶𖬲𞄲𖬳𞄳𖬴𞄰𖬵𞄴𖬶𞄵𞋬-〪𞋯-〭⃐-⃦⃗⃛⃜⃡-𐇽⃩ͣᷲᷓ-ᷖᷧ-ᷩͨᷗͩᷙᷘͤᷪᷫᷚᷛͪͥᷜ-ᷞᷬͫᷟ-᷊ᷡͦᷳᷭᷮͬᷢ-ᷥᷯͭͧᷴᷰͮᷱͯᷦ᷒ⷶⷠ-ⷣⷷꙴⷤⷥꙵꙶⷸⷦ-ⷭⷵⷮꙷⷹꚞⷯꙻⷰ-ⷳꙸ-ꙺⷺ-ⷼꚟⷽ-ⷿⷴ𞀀-𞀆𞀈-𞀘𞀛-𞀡𞀣𞀤𞀦-𞀪𐍶-𐍺ࠖࠗࠛަ-ްऺॖॗु-ॄॢॣॅॕॆ-ैꣿ्ু-ৄৢৣ্ੑੵੁੂੇੈੋ-੍ુ-ૄૢૣૅેૈ્ିୁ-ୄୢୣ୍ୖீ்ా-ీౢౣె-ైొ-్ౕౖಿೢೣೆೌ್ു-ൄൢൣ്഻഼ි-ුූ්ꯥꯨꫬꫭ꯭꫶ꠂ꠆ꠥꠦ꣄𑂳-𑂶𑂹𑆶-𑆾𑈯-𑈱𑋣-𑋨𑋪𑍀𑐸-𑐿𑑂𑒳-𑒸𑒺𑓂𑖲𑗜𑖳𑗝𑖴𑖵𑖿𑘳-𑘺𑘿𑚭𑚰-𑚵𑧔-𑧗𑧚𑧛𑧠𑠯-𑠶𑠹𑜢-𑜥𑜧-𑜫𑜝-𑜟𑵇𑴱-𑴶𑴺𑴼𑴽𑴿𑵄𑵅𑶐𑶑𑶗ᮬᮢᮣᮭᮤᮥᮨᮩ᮫𑀸-𑁆𑁿𐨁-𐨃𐨅𐨆𐨌𐨿𑰰-𑰶𑰸-𑰻𑰿ัิ-ฺັິ-ຼꪰꪲ-ꪴꪷꪸꪾྐྐྵྑ-ྗྙ-ྭྺྮ-ྱྻྲྼླ-ྸྍ-ྏཱ-ཱཱིྀྀུ-྄ཽ𑨻-𑨾𑨁-𑨊𑨴𑩇𑩑-𑩓𑩙𑩚𑩔𑩖𑩕𑩛𑪊-𑪐𑪕𑪑-𑪔𑪙𑲒-𑲧𑲪-𑲰𑲲𑲳ᰶᰬ-ᰳᤠ-ᤢᤧᤨᤲᜒ-᜔ᜲ-᜴ᝒᝓᝲᝳᨘᨗᨛ𑻳𑻴ᯨᯩᯭᯯ-ᯱꥇ-ꥑꤦ-ꤪၞ-ၠွႂှၲိၱီဳုၳၴူၘၙဵႅဲႝဴꧥႆ္်ႍꩼ𑄧-𑄫𑄭-𑄴ិ-ួ្ᩘ-ᩛᩫᩖᩜ-ᩞᩬᩢᩥ-ᩪᩳ᩠ꨵꨶꨩ-ꨮꨱꨲꩃꩌᬶ-ᬺᬼᭂꦼꦶ-ꦹꦽᢅᢆᢩ𞥋𖽏𖾏-𖾒]"
	
	function next_bad_ZWNJ(str, pos)
		pos = pos or 1
		return coroutine.wrap(function ()
			while true do
				local next_pos = str:find(ZWNJ, pos)
				if not next_pos then
					break
				end
				
				local found_left_joining, found_right_joining = false, false
				for char in iter_prev_char(str, next_pos) do
					if Arabic_left_joining:is_match(char) then
						found_left_joining = true
						break
					elseif not transparent:is_match(char) then
						break
					end
				end
				
				for char in iter_next_char(str, next_pos + #ZWNJ) do
					if Arabic_right_joining:is_match(char) then
						found_right_joining = true
						break
					elseif not transparent:is_match(char) then
						break
					end
				end
				
				if not (found_left_joining and found_right_joining) then
					coroutine.yield(next_pos)
					goto continue
				end
				
				::continue::
				pos = next_pos + #ZWNJ
			end
		end)
	end
end

local function collect(iter)
	local vals = Array()
	for val in iter do
		vals:insert(val)
	end
	return vals
end

local count = 0
local limit = type((...)) == "string" and tonumber(...) or math.huge

local Arabic_regex = rure.new "\\p{Arabic}"

--[[
for link, title, template in iterate_templates.iterate_links(assert(io.read 'a')) do
	if ((link.term and Arabic_regex:is_match(link.term) and next_bad_ZWNJ(link.term)() ~= nil)
			or (link.alt and Arabic_regex:is_match(link.alt) and next_bad_ZWNJ(link.alt)() ~= nil)) then
		data_by_title[title]:insert(template.text)
		
		count = count + 1
		if count > limit then
			break
		end
	end
end
--]]

for namespace, title in assert(io.read 'a'):gmatch('%f[^\n\0](%d)\t([^\n]+)') do
	if namespace == "0" and Arabic_regex:is_match(title) and next_bad_ZWNJ(title)() ~= nil then
		title = title:gsub("_", " ")
		print('* <span class="Arab">[[' .. title .. '|' .. title
			.. ']]</span> (<span class="Arab">'
			.. title:gsub(ZWNJ, '<span dir="ltr">&amp;zwnj;</span>') .. '</span>)')
	end
end