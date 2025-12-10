-- To run this sample :
-- bin/soluna.exe entry=test/sprite.lua
local soluna = require "soluna"
local ltask = require "ltask"

soluna.set_window_title "soluna sprite sample"
-- local sprites = soluna.load_sprites "asset/sprites.dl"
local sprites = soluna.load_sprites {
	path = "asset/",
	{
		name = "avatar",
		filename = "avatar.png",
		x = -0.5,
		y = -1,
	}
}

local args = ...
local batch = args.batch

local callback = {}

function callback.frame(count)
	batch:add(sprites.avatar, args.width / 2, args.height/2, 1, 0)
end

return callback

