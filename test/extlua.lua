-- bin/soluna.exe entry=test/extlua.lua

local soluna = require "soluna"
local libs = soluna.extlib "sample"
local foobar = require "ext.foobar"
assert(libs["ext.foobar"] == foobar)
print(foobar.hello())