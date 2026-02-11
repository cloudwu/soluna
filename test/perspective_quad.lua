-- To run this sample :
-- bin/soluna.exe entry=test/perspective_quad.lua
local soluna = require "soluna"
local matpq = require "soluna.material.perspective_quad"
local matquad = require "soluna.material.quad"
local mattext = require "soluna.material.text"
local font = require "soluna.font"
local file = require "soluna.file"

soluna.set_window_title "perspective quad regression matrix"

local sprites = soluna.load_sprites "asset/sprites.dl"
local avatar = assert(sprites.avatar2)

local CARD_W <const> = 160
local CARD_H <const> = 196
local HALF_W <const> = CARD_W * 0.5
local HALF_H <const> = CARD_H * 0.5
local TILE_BG <const> = 0x303845ff
local WHITE <const> = 0xffffffff
local LABEL_TEXT <const> = 0xe7edf5
local LABEL_H <const> = 20

local function load_font()
	if soluna.platform == "wasm" then
		local bundled_path = "asset/font/SourceHanSansSC-Regular.ttf"
		local bundled_data = file.load(bundled_path)
		if bundled_data then
			font.import(bundled_data)
			local bundled_id = font.name "Source Han Sans SC Regular"
			if bundled_id then
				return bundled_id
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
	return nil
end

local fontid = load_font()
local make_label
if fontid then
	make_label = mattext.block(font.cobj(), fontid, 14, LABEL_TEXT, "CV")
end
local label_cache = {}

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

local function draw_tile(batch, x, y, w, h, label)
	local left = x - w * 0.5
	local top = y - h * 0.5
	batch:add(matquad.quad(w, h, TILE_BG), left, top)
	if make_label and label then
		local key = string.format("%s:%d", label, w)
		local text = label_cache[key]
		if text == nil then
			text = make_label(label, w, LABEL_H)
			label_cache[key] = text
		end
		batch:add(text, left, top)
	end
end

local function draw_sprite(batch, x, y, options)
	batch:add(matpq.sprite(avatar, options), x, y)
end

local args = ...
local batch = args.batch
local callback = {}
local base_scale_x <const> = 0.58
local base_scale_y <const> = 0.58

local function draw_flip_cases(t)
	local y = 126
	draw_tile(batch, 110, y, 130, 160, "base")
	draw_sprite(batch, 110, y, { scale_x = base_scale_x, scale_y = base_scale_y, color = WHITE })

	draw_tile(batch, 250, y, 130, 160, "flip_x")
	draw_sprite(batch, 250, y, { scale_x = -base_scale_x, scale_y = base_scale_y, color = WHITE })

	draw_tile(batch, 390, y, 130, 160, "flip_y")
	draw_sprite(batch, 390, y, { scale_x = base_scale_x, scale_y = -base_scale_y, color = WHITE })

	draw_tile(batch, 530, y, 130, 160, "flip_xy")
	draw_sprite(batch, 530, y, {
		scale_x = -base_scale_x,
		scale_y = -base_scale_y,
		color = WHITE,
	})

	draw_tile(batch, 670, y, 130, 160, "affine")
	draw_sprite(batch, 670, y, {
		scale_x = base_scale_x * 1.30,
		scale_y = base_scale_y * 0.74,
		shear_x = math.sin(t * 0.8) * 0.40,
		shear_y = math.cos(t * 0.7) * 0.16,
		color = WHITE,
	})
end

local function draw_perspective_compare(t)
	local y = args.height * 0.56
	local theta = math.sin(t * 0.72) * 1.15
	local quad, q = card_quad(theta)
	local affine_q = { 1.0, 1.0, 1.0, 1.0 }

	draw_tile(batch, args.width * 0.36, y, 220, 250, "q=1")
	draw_sprite(batch, args.width * 0.36, y, {
		quad = quad,
		q = affine_q,
		color = WHITE,
	})

	draw_tile(batch, args.width * 0.64, y, 220, 250, "perspective q")
	draw_sprite(batch, args.width * 0.64, y, {
		quad = quad,
		q = q,
		color = WHITE,
	})
end

local function draw_arbitrary_quad_cases(t)
	local y = args.height - 120
	local wobble = math.sin(t * 1.1)
	local quad_a = {
		-80, -72,
		70, -52,
		-64, 68,
		78, 82,
	}
	local quad_b = {
		-88, -58,
		84, -90 + wobble * 10,
		-74, 92,
		82, 54 + wobble * 24,
	}

	draw_tile(batch, 240, y, 220, 200, "irregular")
	draw_sprite(batch, 240, y, {
		quad = quad_a,
		q = { 1.0, 1.0, 1.0, 1.0 },
		color = WHITE,
	})

	draw_tile(batch, 490, y, 220, 200, "irregular + q")
	draw_sprite(batch, 490, y, {
		quad = quad_b,
		q = { 1.0, 1.25, 0.86, 1.42 },
		color = WHITE,
	})
end

local function draw_degenerate_and_extreme_q(t)
	local y = args.height - 120
	local s = math.sin(t * 0.9)
	local sliver = {
		-90, -6,
		90, -8 + s * 2.0,
		-86, 8,
		88, 6 + s * 2.0,
	}

	draw_tile(batch, 740, y, 220, 200, "degenerate/extreme q")
	draw_sprite(batch, 740, y, {
		quad = sliver,
		q = { 1.0, 0.0, -3.0, 0.00000001 },
		color = WHITE,
	})
end

function callback.frame(count)
	local t = count * 0.03

	draw_flip_cases(t)
	draw_perspective_compare(t)
	draw_arbitrary_quad_cases(t)
	draw_degenerate_and_extreme_q(t)
end

return callback
