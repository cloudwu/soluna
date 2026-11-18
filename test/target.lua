local soluna = require "soluna"
local matquad = require "soluna.material.quad"

soluna.set_window_title "soluna render target sample"

local function checker(n, size, a, b)
	local t = {}
	for y = 0, n * size - 1 do
		for x = 0, n * size - 1 do
			t[#t + 1] = (x // size + y // size) % 2 == 0 and a or b
		end
	end
	return table.concat(t)
end

soluna.preload {
	filename = "@checker",
	content = checker(8, 12, "\xff\xff\xff\xff", "\x40\x40\x40\xff"),
	w = 96,
	h = 96,
}

local rects = soluna.load_sprites {
	{
		name = "checker",
		filename = "@checker",
	},
}

local scene_pass, scene = soluna.target(480, 360, 0x203040)
local small_pass, small = soluna.target(160, 120)

local args = ...
local batch = args.batch

local callback = {}

local function shrink()
	batch:pass(small_pass)
	batch:layer(1 / 3, 0, 0)
		batch:add(scene)
	batch:layer()
end

function callback.frame(count)
	local rad = count * math.pi / 180
	-- each target is entered twice in a frame; the second time keeps what the first drew
	batch:pass(scene_pass)
	batch:layer(180, 180)
		batch:layer(rad)
			batch:add(matquad.quad(180, 180, 0xff8040), -90, -90)
		batch:layer()
	batch:layer()
	shrink()
	batch:pass(scene_pass)
	batch:add(rects.checker, 336, 36)
	shrink()
	batch:pass(1)
	local y = (args.height - 360) // 2
	-- left : draw the target as a sprite
	batch:add(scene, 16, y)
	-- right : shrink the target to a third, then draw it three times larger
	batch:layer(3, 528, y)
		batch:add(small)
	batch:layer()
end

return callback
