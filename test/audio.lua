local soluna = require "soluna"
local matquad = require "soluna.material.quad"
local mattext = require "soluna.material.text"
local font = require "soluna.font"
local file = require "soluna.file"

soluna.load_sounds "asset/sounds.dl"
soluna.set_window_title "Soluna sound sample"

local args = ...
local batch = args.batch
local screen_w = args.width
local screen_h = args.height

local BUTTON_W <const> = 180
local BUTTON_H <const> = 64
local SHADOW_Y <const> = 6

local pointer_x = screen_w // 2
local pointer_y = screen_h // 2
local pressed = false

local function load_font(data, name)
	if not data then
		return
	end
	font.import(data)
	return font.name(name or "")
end

local function font_init()
	if soluna.platform == "wasm" then
		local fontid = load_font(file.load "asset/font/SourceHanSansSC-Regular.ttf", "Source Han Sans SC Regular")
		if fontid then
			return fontid
		end
	end

	local sysfont = require "soluna.font.system"
	for _, name in ipairs {
		"WenQuanYi Micro Hei",
		"Microsoft YaHei",
		"Yuanti SC",
		"Source Han Sans SC Regular",
	} do
		local ok, data = pcall(sysfont.ttfdata, name)
		local fontid = ok and load_font(data, name)
		if fontid then
			return fontid
		end
	end
	error "No available system font for audio sample"
end

local play_label = mattext.block(font.cobj(), font_init(), 28, 0xff28435c, "C")("Play", BUTTON_W, BUTTON_H)

local function update_pointer(x, y)
	pointer_x = x
	pointer_y = y
end

local function inside_button(x, y)
	local bx = (screen_w - BUTTON_W) // 2
	local by = (screen_h - BUTTON_H) // 2
	return x >= bx and x <= bx + BUTTON_W and y >= by and y <= by + BUTTON_H
end

local callback = {}

function callback.window_resize(w, h)
	screen_w = w
	screen_h = h
end

function callback.mouse_move(x, y)
	update_pointer(x, y)
end

function callback.mouse_button(button, key_state)
	if button ~= 0 then
		return
	end
	if key_state == 1 then
		pressed = inside_button(pointer_x, pointer_y)
		return
	end
	if pressed and inside_button(pointer_x, pointer_y) then
		soluna.play_sound "bloop"
	end
	pressed = false
end

function callback.touch_begin(x, y)
	update_pointer(x, y)
	pressed = inside_button(x, y)
end

function callback.touch_moved(x, y)
	update_pointer(x, y)
	pressed = inside_button(x, y)
end

function callback.touch_end(x, y)
	update_pointer(x, y)
	if pressed and inside_button(x, y) then
		soluna.play_sound "bloop"
	end
	pressed = false
end

function callback.touch_cancelled()
	pressed = false
end

function callback.frame()
	local bx = (screen_w - BUTTON_W) // 2
	local by = (screen_h - BUTTON_H) // 2
	local hovered = inside_button(pointer_x, pointer_y)
	local face_y = by + (pressed and 4 or 0)
	local color = pressed and 0xffcfd8e4 or hovered and 0xfffbfdff or 0xffeef3f8
	batch:add(matquad.quad(BUTTON_W, BUTTON_H, 0xff7389a3), bx, by + SHADOW_Y)
	batch:add(matquad.quad(BUTTON_W, BUTTON_H, color), bx, face_y)
	batch:add(play_label, bx, face_y)
end

return callback
