local lm = require "luamake"

lm.rootdir = lm.basedir .. "/3rd/yoga"

lm:source_set "yoga_src" {
	sources = {
		"yoga/*.cpp",
		"yoga/*/*.cpp",
	},
	includes = {
		lm.rootdir,
	}
}
