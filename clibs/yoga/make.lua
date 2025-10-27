local lm = require "luamake"
local fs = require "bee.filesystem"

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
