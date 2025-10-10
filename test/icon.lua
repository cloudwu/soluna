local image = require "soluna.image"
local file = require "soluna.file"
local soluna = require "soluna"

local c = file.load "asset/lua-logo.png"
local content, w, h = image.load(c)
soluna.set_icon({ data = content, w = w, h = h })

local callback = {}

function callback.frame(count)
end

return callback
