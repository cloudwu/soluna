local lm = require "luamake"

lm.rootdir = lm.basedir .. "/3rd/lua"

if lm.os == "windows" then
	lm:source_set "winfile" {
		sources = {
			lm.basedir .. "/src/winfile.c",
		},
	}
end

lm:source_set "lua_src" {
	sources = {
		"onelua.c",
	},
	defines = {
		"MAKE_LIB",
		lm.os == "windows" and "LUA_DL_DLL" or "LUA_USE_DLOPEN",
	},
}

lm:exe "lua" {
	deps = {
		lm.os == "windows" and "winfile",
	},
	sources = {
		"onelua.c",
	},
	defines = {
		"MAKE_LUA",
	},
	windows = {
		defines = {
			"fopen=fopen_utf8",
		},
	},
}
