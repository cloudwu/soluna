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

local sound_bus = assert(soluna.audio_bus "sound")
local music_bus = assert(soluna.audio_bus "music")

local BUTTON_W <const> = 220
local BUTTON_H <const> = 56
local BUTTON_GAP <const> = 18
local BUTTON_COLS <const> = 2
local BUTTON_ROWS <const> = 5
local SHADOW_Y <const> = 6
local BUTTON_TEXT_COLOR <const> = 0xff28435c
local STATUS_TEXT_COLOR <const> = 0xffdce7f2

local BUTTON_PLAY_SHOT <const> = 1
local BUTTON_TOGGLE_LOOP <const> = 2
local BUTTON_STOP_LAST <const> = 3
local BUTTON_STOP_LOOP <const> = 4
local BUTTON_SEEK_START <const> = 5
local BUTTON_SEEK_STEP <const> = 6
local BUTTON_SOUND_DOWN <const> = 7
local BUTTON_SOUND_UP <const> = 8
local BUTTON_MUSIC_DOWN <const> = 9
local BUTTON_MUSIC_UP <const> = 10

local pointer_x = screen_w // 2
local pointer_y = screen_h // 2
local pressed = false
local pressed_button

local sound_volume = 1.0
local music_volume = 1.0
local last_voice
local loop_voice
local loop_time = 0.0

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

local fontid = font_init()
local button_text = mattext.block(font.cobj(), fontid, 24, BUTTON_TEXT_COLOR, "CV")
local info_text = mattext.block(font.cobj(), fontid, 20, STATUS_TEXT_COLOR, "L")

local labels = {
	[BUTTON_PLAY_SHOT] = button_text("Play Shot", BUTTON_W, BUTTON_H),
	[BUTTON_TOGGLE_LOOP] = button_text("Toggle Loop", BUTTON_W, BUTTON_H),
	[BUTTON_STOP_LAST] = button_text("Stop Last Voice", BUTTON_W, BUTTON_H),
	[BUTTON_STOP_LOOP] = button_text("Stop Loop Voice", BUTTON_W, BUTTON_H),
	[BUTTON_SEEK_START] = button_text("Seek Loop 0.00", BUTTON_W, BUTTON_H),
	[BUTTON_SEEK_STEP] = button_text("Seek Loop +0.20", BUTTON_W, BUTTON_H),
	[BUTTON_SOUND_DOWN] = button_text("Sound -", BUTTON_W, BUTTON_H),
	[BUTTON_SOUND_UP] = button_text("Sound +", BUTTON_W, BUTTON_H),
	[BUTTON_MUSIC_DOWN] = button_text("Music -", BUTTON_W, BUTTON_H),
	[BUTTON_MUSIC_UP] = button_text("Music +", BUTTON_W, BUTTON_H),
}

local function clamp(v, lo, hi)
	if v < lo then
		return lo
	elseif v > hi then
		return hi
	end
	return v
end

local function playing(voice)
	return voice ~= nil and voice:playing()
end

local function update_pointer(x, y)
	pointer_x = x
	pointer_y = y
end

local function button_rect(index)
	local total_w = BUTTON_COLS * BUTTON_W + (BUTTON_COLS - 1) * BUTTON_GAP
	local total_h = BUTTON_ROWS * BUTTON_H + (BUTTON_ROWS - 1) * BUTTON_GAP
	local origin_x = (screen_w - total_w) // 2
	local origin_y = (screen_h - total_h) // 2 - 40
	local col = (index - 1) % BUTTON_COLS
	local row = (index - 1) // BUTTON_COLS
	return origin_x + col * (BUTTON_W + BUTTON_GAP), origin_y + row * (BUTTON_H + BUTTON_GAP)
end

local function button_at(x, y)
	for i = 1, BUTTON_ROWS * BUTTON_COLS do
		local bx, by = button_rect(i)
		if x >= bx and x <= bx + BUTTON_W and y >= by and y <= by + BUTTON_H then
			return i
		end
	end
end

local function click_button(index)
	if index == BUTTON_PLAY_SHOT then
		last_voice = assert(soluna.play_sound("bloop", {
			volume = 0.25,
			pan = clamp(pointer_x / screen_w * 2.0 - 1.0, -1.0, 1.0),
			pitch = 0.95,
		}))
		return
	end

	if index == BUTTON_TOGGLE_LOOP then
		local voice = loop_voice
		if playing(voice) then
			voice:stop(0.1)
			if loop_voice == voice then
				loop_voice = nil
			end
		else
			loop_voice = assert(soluna.play_sound "bloop_loop")
		end
		return
	end

	if index == BUTTON_STOP_LAST then
		local voice = last_voice
		if voice then
			voice:stop()
			if last_voice == voice then
				last_voice = nil
			end
		end
		return
	end

	if index == BUTTON_STOP_LOOP then
		local voice = loop_voice
		if voice then
			voice:stop(0.1)
			if loop_voice == voice then
				loop_voice = nil
				loop_time = 0.0
			end
		end
		return
	end

	if index == BUTTON_SEEK_START then
		local voice = loop_voice
		if voice then
			voice:seek(0.0)
		end
		return
	end

	if index == BUTTON_SEEK_STEP then
		local voice = loop_voice
		if voice then
			local now = voice:tell() or 0.0
			voice:seek(now + 0.2)
		end
		return
	end

	if index == BUTTON_SOUND_DOWN then
		sound_volume = clamp(sound_volume - 0.1, 0.0, 1.0)
		sound_bus:set_volume(sound_volume)
		return
	end

	if index == BUTTON_SOUND_UP then
		sound_volume = clamp(sound_volume + 0.1, 0.0, 1.0)
		sound_bus:set_volume(sound_volume)
		return
	end

	if index == BUTTON_MUSIC_DOWN then
		music_volume = clamp(music_volume - 0.1, 0.0, 1.0)
		music_bus:set_volume(music_volume)
		return
	end

	if index == BUTTON_MUSIC_UP then
		music_volume = clamp(music_volume + 0.1, 0.0, 1.0)
		music_bus:set_volume(music_volume)
	end
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
		pressed_button = button_at(pointer_x, pointer_y)
		pressed = pressed_button ~= nil
		return
	end
	local index = button_at(pointer_x, pointer_y)
	if pressed and index == pressed_button then
		click_button(index)
	end
	pressed = false
	pressed_button = nil
end

function callback.touch_begin(x, y)
	update_pointer(x, y)
	pressed_button = button_at(x, y)
	pressed = pressed_button ~= nil
end

function callback.touch_moved(x, y)
	update_pointer(x, y)
	if button_at(x, y) ~= pressed_button then
		pressed = false
	end
end

function callback.touch_end(x, y)
	update_pointer(x, y)
	local index = button_at(x, y)
	if pressed and index == pressed_button then
		click_button(index)
	end
	pressed = false
	pressed_button = nil
end

function callback.touch_cancelled()
	pressed = false
	pressed_button = nil
end

function callback.frame()
	local last = last_voice
	local loop = loop_voice
	local last_playing = playing(last)
	local loop_playing = playing(loop)

	if loop_playing then
		loop_time = loop:tell() or loop_time
	else
		loop_time = 0.0
	end

	local title = info_text("Audio API Sample", 400, 28)
	local subtitle = info_text("Play voices, seek a stream voice, and adjust sound/music buses.", 640, 24)
	local status_1 = info_text(
		string.format("sound bus %.1f  |  music bus %.1f", sound_volume, music_volume),
		480,
		24
	)
	local status_2 = info_text(
		string.format(
			"last voice %s  |  loop voice %s  |  loop time %.2f",
			last_playing and "playing" or "idle",
			loop_playing and "playing" or "idle",
			loop_time
		),
		640,
		24
	)

	batch:add(title, (screen_w - 400) // 2, 40)
	batch:add(subtitle, (screen_w - 640) // 2, 72)
	batch:add(status_1, (screen_w - 480) // 2, screen_h - 90)
	batch:add(status_2, (screen_w - 640) // 2, screen_h - 62)

	for i = 1, BUTTON_ROWS * BUTTON_COLS do
		local bx, by = button_rect(i)
		local hovered = button_at(pointer_x, pointer_y) == i
		local active = pressed and pressed_button == i
		local face_y = by + (active and 4 or 0)
		local face_color = active and 0xffcfd8e4 or hovered and 0xfffbfdff or 0xffeef3f8
		batch:add(matquad.quad(BUTTON_W, BUTTON_H, 0xff7389a3), bx, by + SHADOW_Y)
		batch:add(matquad.quad(BUTTON_W, BUTTON_H, face_color), bx, face_y)
		batch:add(labels[i], bx, face_y)
	end
end

return callback
