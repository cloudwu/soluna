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
	},
	linux = {
		links = { "dl" },
		defines = { "LUA_USE_LINUX" },
	},
	windows = {
		defines = {
			"LUA_USE_WINDOWS",
		},
	},
	macos = {
		defines = { "LUA_USE_MACOSX" },
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
	macos = {
		defines = { "LUA_USE_MACOSX" },
	},
	linux = {
		links = { "dl" },
		defines = { "LUA_USE_LINUX" },
	},
	windows = {
		defines = {
			"fopen=fopen_utf8",
			"LUA_USE_WINDOWS",
		},
	},
}
