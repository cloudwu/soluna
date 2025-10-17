local ttf = require "soluna.font.truetype"
local string = string
local utf8 = utf8
local table = table
local debug = debug

global pairs, ipairs, assert, rawget, setmetatable

local MAXFONT <const> = 64

local namelist = {}

local CACHE = {}

local function utf16toutf8(s)
	local surrogate
	return (s:gsub("..", function(utf16)
		local cp = string.unpack(">H", utf16)
		if (cp & 0xFC00) == 0xD800 then
			surrogate = cp
			return ""
		else
			if surrogate then
				cp = ((surrogate - 0xD800) << 10) + (cp - 0xDC00) + 0x10000
				surrogate = nil
			end
			return utf8.char(cp)
		end
	end))
end

local ids = {
	UNICODE = {
		id = 0,
		encoding = {
			UNICODE_1_0 = 0,
			UNICODE_1_1 = 1,
			ISO_10646 = 2,
			UNICODE_2_0_BMP = 3,
			UNICODE_2_0_FULL = 4,
		},
		lang = {
			default = 0,
			ENGLISH = 0,
			CHINESE = 1,
			FRENCH = 2,
			GERMAN = 3,
			JAPANESE = 4,
			KOREAN = 5,
			SPANISH = 6,
			ITALIAN = 7,
			DUTCH = 8,
			SWEDISH = 9,
			RUSSIAN = 10,
		},
	},
	MICROSOFT = {
		id = 3,
		encoding = {
			UNICODE_BMP = 1,
			UNICODE_FULL = 10,
		},
		lang = {
			ENGLISH     =0x0409,
			CHINESE     =0x0804,
			DUTCH       =0x0413,
			FRENCH      =0x040c,
			GERMAN      =0x0407,
			HEBREW      =0x040d,
			ITALIAN     =0x0410,
			JAPANESE    =0x0411,
			KOREAN      =0x0412,
			RUSSIAN     =0x0419,
			SPANISH     =0x0409,
			SWEDISH     =0x041D,
		},
	},
  MACINTOSH = {
		id = 1,
		encoding = {
			ROMAN = 0,
			JAPANESE = 1,
			CHINESE_TRADITIONAL = 2,
			KOREAN = 3,
			ARABIC = 4,
			HEBREW = 5,
			GREEK = 6,
			RUSSIAN = 7,
			RSYMBOL = 8,
			DEVANAGARI = 9,
			GURMUKHI = 10,
			GUJARATI = 11,
			ORIYA = 12,
			BENGALI = 13,
			TAMIL = 14,
			TELUGU = 15,
			KANNADA = 16,
			MALAYALAM = 17,
			SINHALESE = 18,
			BURMESE = 19,
			KHMER = 20,
			THAI = 21,
			LAOTIAN = 22,
			GEORGIAN = 23,
			ARMENIAN = 24,
			CHINESE_SIMPLIFIED = 25,
			TIBETAN = 26,
			MONGOLIAN = 27,
			GEEZ = 28,
			SLAVIC = 29,
			VIETNAMESE = 30,
			SINDHI = 31,
		},
		lang = {
			ENGLISH = 0,
			FRENCH = 1,
			GERMAN = 2,
			ITALIAN = 3,
			DUTCH = 4,
			SWEDISH = 5,
			SPANISH = 6,
			DANISH = 7,
			PORTUGUESE = 8,
			NORWEGIAN = 9,
			HEBREW = 10,
			JAPANESE = 11,
			ARABIC = 12,
			FINNISH = 13,
			GREEK = 14,
			ICELANDIC = 15,
			MALTESE = 16,
			TURKISH = 17,
			CROATIAN = 18,
			CHINESE_TRADITIONAL = 19,
			URDU = 20,
			HINDI = 21,
			THAI = 22,
			KOREAN = 23,
			LITHUANIAN = 24,
			POLISH = 25,
			HUNGARIAN = 26,
			ESTONIAN = 27,
			LATVIAN = 28,
			SAMI = 29,
			FAROESE = 30,
			FARSI = 31,
			RUSSIAN = 32,
			CHINESE_SIMPLIFIED = 33,
			FLEMISH = 34,
			IRISH_GAELIC = 35,
			ALBANIAN = 36,
			ROMANIAN = 37,
			CZECH = 38,
			SLOVAK = 39,
			SLOVENIAN = 40,
			YIDDISH = 41,
			SERBIAN = 42,
			MACEDONIAN = 43,
			BULGARIAN = 44,
			UKRAINIAN = 45,
			BYELORUSSIAN = 46,
			UZBEK = 47,
			KAZAKH = 48,
			AZERBAIJANI_CYRILLIC = 49,
			AZERBAIJANI_ARABIC = 50,
			ARMENIAN = 51,
			GEORGIAN = 52,
			MOLDAVIAN = 53,
			KIRGHIZ = 54,
			TAJIKI = 55,
			TURKMEN = 56,
			MONGOLIAN = 57,
			MONGOLIAN_CYRILLIC = 58,
			PASHTO = 59,
			KURDISH = 60,
			KASHMIRI = 61,
			SINDHI = 62,
			TIBETAN = 63,
			NEPALI = 64,
			SANSKRIT = 65,
			MARATHI = 66,
			BENGALI = 67,
			ASSAMESE = 68,
			GUJARATI = 69,
			PUNJABI = 70,
			ORIYA = 71,
			MALAYALAM = 72,
			KANNADA = 73,
			TAMIL = 74,
			TELUGU = 75,
			SINHALESE = 76,
			BURMESE = 77,
			KHMER = 78,
			LAO = 79,
			VIETNAMESE = 80,
			INDONESIAN = 81,
			TAGALOG = 82,
			MALAY_ROMAN = 83,
			MALAY_ARABIC = 84,
			AMHARIC = 85,
			TIGRINYA = 86,
			GALLA = 87,
			SOMALI = 88,
			SWAHILI = 89,
			KINYARWANDA = 90,
			RUNDI = 91,
			NYANJA = 92,
			MALAGASY = 93,
			ESPERANTO = 94,
		},
	},
}

local function import(fontdata)
	local index = 0
	local cache = {}
	while true do
		for _, obj in pairs(ids) do
			for _, encoding_id in pairs(obj.encoding) do
				for _, lang_id in pairs(obj.lang) do
					local fname, sname = ttf.namestring(fontdata, index, obj.id, encoding_id, lang_id)
					if fname then
						fname = utf16toutf8(fname)
						local fullname = fname
						fname = string.lower(fname)
						if sname then
						  sname = utf16toutf8(sname)
						  fullname = fullname .. " " .. sname
						  sname = string.lower(sname)
						end
						if not cache[fullname] then
							cache[fullname] = true
							table.insert(namelist, {
								fontdata = fontdata,
								index = index,
								family = fname,
								sfamily = sname,	-- sub family name
								name = string.lower(fullname),
							})
						end
					elseif fname == nil then
						return
					end
				end
			end
		end
		index = index + 1
	end
end

local FONT_ID = 0
local function alloc_fontid()
	FONT_ID = FONT_ID + 1
	assert(FONT_ID <= MAXFONT)
	return FONT_ID
end

local function matching(obj, name)
	if obj.family == name or obj.name == name then
		return true
	end
end

local function fetch_name(nametable, name_)
	if name_ == "" and namelist[1] then
		name_ = namelist[1].name
		local id = rawget(nametable, name_)
		if id then
			nametable[""] = id
			return id
		end
	end
	local name = string.lower(name_)
	for _, obj in ipairs(namelist) do
		if matching(obj, name) then
			if not obj.id then
				obj.id = alloc_fontid()
				CACHE[obj.id] = obj
			end

			local id = obj.id
			nametable[name_] = id
			return id
		end
	end
end

setmetatable(ttf.nametable, { __index = fetch_name })

local function fetch_id(_, id)
	local obj = assert(CACHE[id])
	return ttf.update(id, obj.fontdata, obj.index)
end

setmetatable(ttf.idtable, { __index = fetch_id })

local function enum_name(_, idx)
	local i = idx // 2
	local what = (idx % 2 == 1) and "family" or "name"
	local n = namelist[i+1]
	if not n then
		return
	else
		return n[what]
	end
end

setmetatable(ttf.enum, { __index = enum_name })

debug.getregistry().TRUETYPE_IMPORT = import
