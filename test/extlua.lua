-- bin/soluna.exe test/extlua.game

local soluna = require "soluna"
local font = require "soluna.font"
local file = require "soluna.file"
local foobar = require "ext.foobar"
local font_probe = require "ext.font_probe"
local matpq = require "ext.material.perspective_quad"

print(foobar.hello())
soluna.set_window_title "extlua perspective quad"

local function init_font()
	if soluna.platform == "wasm" then
		local data = file.load "asset/font/SourceHanSansSC-Regular.ttf"
		if data then
			font.import(data)
			local fontid = font.name "Source Han Sans SC Regular"
			if fontid then
				return fontid
			end
		end
	end

	local sysfont = require "soluna.font.system"
	local candidates = {
		"WenQuanYi Micro Hei",
		"Microsoft YaHei",
		"Yuanti SC",
		"Source Han Sans SC Regular",
	}
	for _, name in ipairs(candidates) do
		local ok, data = pcall(sysfont.ttfdata, name)
		if ok and data then
			font.import(data)
			local fontid = font.name(name)
			if fontid then
				return fontid
			end
		end
	end
	error "No available system font for extlua fontapi sample"
end

local fontid = init_font()
local info = font_probe.info(font.cobj(), fontid, 32, string.byte "A")
print("fontapi", info.texture_size, info.edge, info.ascent, info.advance_x, info.width, info.height)

local args = ...
local batch = args.batch
local callback = {}

local CARD_W <const> = 160
local CARD_H <const> = 196
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
			x = -0.5,
			y = -0.5,
		},
	}
end

local sprites = make_card_sprite()
local card = assert(sprites.card)

function callback.frame(count)
	local theta = math.sin(count * 0.021) * 1.15
	batch:add(matpq.sprite(card, {
		sin_angle = math.sin(theta),
		cos_angle = math.cos(theta),
		color = WHITE,
	}), args.width * 0.5, args.height * 0.5)
end

return callback
