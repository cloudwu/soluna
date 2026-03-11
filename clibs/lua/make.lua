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
		"LUA_USE_DLOPEN",
	},
	linux = {
		links = { "dl" },
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
		"LUA_USE_DLOPEN",
	},
	linux = {
		links = { "dl" },
	},
	windows = {
		defines = {
			"fopen=fopen_utf8",
		},
	},
}
