local image = require "soluna.image"
local file = require "soluna.file"

local c = file.load "asset/avatar.png"
print(image.info(c))
local content, w, h = image.load(c)
local x, y, cw, ch = image.crop(content, w, h)
local img = image.new(cw,ch)
local src_rect = image.canvas(content, w, h, x, y, cw, ch)
image.blit(img:canvas(), src_rect)
--img:write("crop.png")

