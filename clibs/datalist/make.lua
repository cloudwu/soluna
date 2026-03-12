local lm = require "luamake"

lm.rootdir = lm.basedir .. "/3rd/datalist"

lm:source_set "datalist_src" {
	sources = {
		"datalist.c",
	},
	includes = {
		lm.basedir .. "/3rd/lua",
	},
}
