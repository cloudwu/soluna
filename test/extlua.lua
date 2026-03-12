-- bin/soluna.exe test/extlua.game

local soluna = require "soluna"
local foobar = require "ext.foobar"
print(foobar.hello())
