local soluna = require "soluna"
local matquad = require "soluna.material.quad"
local matmask = require "soluna.material.mask"
local mattarget = require "soluna.material.target"

soluna.set_window_title "soluna render target sample"

local function checker(n, a, b)
	local t = {}
	for y = 0, n - 1 do
		for x = 0, n - 1 do
			t[#t + 1] = (x + y) % 2 == 0 and a or b
		end
	end
	return table.concat(t)
end

soluna.preload {
	filename = "@checker",
	content = checker(8, "\xff\xff\xff\xff", "\x40\x40\x40\xff"),
	w = 8,
	h = 8,
}

local rects = soluna.load_sprites {
	{
		name = "checker",
		filename = "@checker",
	},
}

local scene = soluna.target(160, 120)

local args = ...
local batch = args.batch

local callback = {}

local function draw(rad)
	batch:layer(60, 60)
		batch:layer(rad)
			batch:add(matquad.quad(60, 60, 0xff8040), -30, -30)
		batch:layer()
	batch:layer()
	batch:layer(4, 112, 12)
		batch:add(rects.checker)
	batch:layer()
end

function callback.frame(count)
	local rad = count * math.pi / 180
	batch:add(mattarget.target(scene))
		draw(rad)
	batch:add(mattarget.target())
	local y = (args.height - 360) // 2
	-- left : draw to the screen directly
	batch:add(matquad.quad(480, 360, 0x203040), 16, y)
	batch:layer(3, 16, y)
		draw(rad)
	batch:layer()
	-- right : draw the target as a sprite, and use it as a mask for the shadow
	batch:add(matquad.quad(480, 360, 0x203040), 528, y)
	batch:layer(3, 528, y)
		batch:add(matmask.mask(scene, 0x80000000), 3, 3)
		batch:add(scene)
	batch:layer()
end

return callback
