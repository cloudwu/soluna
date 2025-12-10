-- To run this sample :
-- bin/soluna.exe entry=test/sprite.lua
local soluna = require "soluna"
local ltask = require "ltask"

soluna.set_window_title "soluna sprite sample"
local sprites = soluna.load_sprites "asset/sprites.dl"

soluna.preload {
	{
		filename = "@red",
		content = "\xff\0\0\xff",
		w = 1,
		h = 1,
	},
	{
		filename = "@green",
		content = "\0\xff\0\xff",
		w = 1,
		h = 1,
	},
}

local rects = soluna.load_sprites {
	{
		name = "red",
		filename = "@red",
	},
	{
		name = "green",
		filename = "@green",
	}
}

local args = ...
local batch = args.batch

local callback = {}
local rot = 0
local delta = math.rad(1)
function callback.frame(count)
	batch:layer(100, args.width/2 , args.height/2)
	batch:layer(rot)
	batch:add(rects.red)
	batch:layer()
	batch:layer(-rot)
	batch:add(rects.green)
	batch:layer()
	batch:layer()
	rot = rot + delta
	batch:add(sprites.avatar, args.width / 2, args.height/2)
end

return callback

