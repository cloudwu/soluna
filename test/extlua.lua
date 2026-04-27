-- bin/soluna.exe test/extlua.game

local soluna = require "soluna"
local foobar = require "ext.foobar"
local matpq = require "ext.material.perspective_quad"

print(foobar.hello())
soluna.set_window_title "extlua perspective quad"

local args = ...
local batch = args.batch
local callback = {}

local CARD_W <const> = 160
local CARD_H <const> = 196
local HALF_W <const> = CARD_W * 0.5
local HALF_H <const> = CARD_H * 0.5
local WHITE <const> = 0xffffffff

local function rgba(color)
	local a = color >> 24 & 0xff
	local r = color >> 16 & 0xff
	local g = color >> 8 & 0xff
	local b = color & 0xff
	return string.pack("BBBB", r, g, b, a)
end

local function create_canvas(width, height)
	local pixels = {}
	local clear = rgba(0)
	for i = 1, width * height do
		pixels[i] = clear
	end

	local canvas = {}

	function canvas.set_pixel(x, y, color)
		if x < 0 or x >= width or y < 0 or y >= height then
			return
		end
		pixels[y * width + x + 1] = rgba(color)
	end

	function canvas.to_content()
		return table.concat(pixels)
	end

	return canvas
end

local function make_card_sprite()
	local canvas = create_canvas(CARD_W, CARD_H)

	for y = 0, CARD_H - 1 do
		for x = 0, CARD_W - 1 do
			local r = 40 + x * 120 // (CARD_W - 1)
			local g = 56 + y * 140 // (CARD_H - 1)
			local b = 224 - y * 72 // (CARD_H - 1)
			canvas.set_pixel(x, y, 0xff000000 | r << 16 | g << 8 | b)
		end
	end

	for y = 0, CARD_H - 1 do
		for x = 0, CARD_W - 1 do
			if x < 3 or x >= CARD_W - 3 or y < 3 or y >= CARD_H - 3 then
				canvas.set_pixel(x, y, 0xffffffff)
			elseif x % 32 == 0 or y % 32 == 0 then
				canvas.set_pixel(x, y, 0x80ffffff)
			end
		end
	end

	soluna.preload {
		filename = "@extlua_perspective_card",
		content = canvas.to_content(),
		w = CARD_W,
		h = CARD_H,
	}

	return soluna.load_sprites {
		{
			name = "card",
			filename = "@extlua_perspective_card",
		},
	}
end

local sprites = make_card_sprite()
local card = assert(sprites.card)

local function card_quad(theta)
	local dist = 460.0
	local focal = 460.0
	local c = math.cos(theta)
	local s = math.sin(theta)
	local corners = {
		{ -HALF_W, -HALF_H },
		{ HALF_W,  -HALF_H },
		{ -HALF_W, HALF_H },
		{ HALF_W,  HALF_H },
	}

	local quad = {}
	local q = {}
	for i = 1, 4 do
		local x = corners[i][1]
		local y = corners[i][2]
		local rx = x * c
		local rz = -x * s
		local w = dist + rz
		local scale = focal / w

		quad[#quad + 1] = rx * scale
		quad[#quad + 1] = y * scale
		q[i] = 1.0 / w
	end
	return quad, q
end

function callback.frame(count)
	local theta = math.sin(count * 0.021) * 1.15
	local quad, q = card_quad(theta)
	batch:add(matpq.sprite(card, {
		quad = quad,
		q = q,
		color = WHITE,
	}), args.width * 0.5, args.height * 0.5)
end

return callback
