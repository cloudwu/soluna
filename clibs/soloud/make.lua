local lm = require "luamake"

lm.rootdir = lm.basedir .. "/3rd/soloud"

lm:source_set "soloud_src" {
	sources = {
		lm.basedir .. "/src/soloudone.cpp",
	},
	includes = {
		"include",
		"src",
	},
	windows = {
		defines = {
			"WITH_WINMM=1",
		},
	},
	macos = {
		defines = {
			"WITH_COREAUDIO=1",
		},
	},
	linux = {
		defines = {
			lm.platform == "emcc" and "WITH_SDL2_STATIC=1" or "WITH_ALSA=1",
		},
	},
}
