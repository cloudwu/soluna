local lm = require "luamake"

lm.rootdir = lm.basedir .. "/3rd/zlib"

lm:source_set "minizip" {
  sources = {
    "contrib/minizip/ioapi.c",
		"contrib/minizip/unzip.c",
		"contrib/minizip/zip.c",
  },
  windows = {
    sources = {
      "contrib/minizip/iowin32.c",
    },
    includes = {
      "contrib/minizip",
    },
  },
  includes = {
    lm.rootdir,
  },

}

lm:source_set "zlib" {
  sources = {
    "*.c",
    "!gz*.c",
  },
}
