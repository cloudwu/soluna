local soluna = require "soluna"
local matquad = require "soluna.material.quad"
local mattarget = require "soluna.material.target"

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

local scene = soluna.target(480, 360, 0x203040)
local small = soluna.target(160, 120)

local args = ...
local batch = args.batch

local callback = {}

local function draw(rad)
	batch:layer(180, 180)
		batch:layer(rad)
			batch:add(matquad.quad(180, 180, 0xff8040), -90, -90)
		batch:layer()
	batch:layer()
	batch:add(rects.checker, 336, 36)
end

function callback.frame(count)
	local rad = count * math.pi / 180
	batch:add(mattarget.target(scene))
		draw(rad)
	batch:add(mattarget.target())
	batch:add(mattarget.target(small))
		batch:layer(1 / 3, 0, 0)
			batch:add(scene)
		batch:layer()
	batch:add(mattarget.target())
	local y = (args.height - 360) // 2
	-- left : draw the target as a sprite
	batch:add(scene, 16, y)
	-- right : shrink the target to a third, then draw it three times larger
	batch:layer(3, 528, y)
		batch:add(small)
	batch:layer()
end

return callback
