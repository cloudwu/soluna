local lm = require "luamake"

lm.rootdir = lm.basedir .. "/3rd/ltask"

lm:source_set "ltask_src" {
	sources = {
		"src/*.c",
	},
	includes = {
		lm.basedir .. "/3rd/lua",
	},
	defines = {
		"LTASK_EXTERNAL_OPENLIBS=soluna_openlibs",
	},
}
