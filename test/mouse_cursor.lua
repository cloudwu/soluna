local app = require "soluna.app"
local soluna = require "soluna"
local quad = require "soluna.material.quad"

local args = ...
local batch = args.batch

local KEY_ESCAPE <const> = 256
local KEYSTATE_PRESS <const> = 1
local TILE_W <const> = 132
local TILE_H <const> = 76
local GAP <const> = 18
local PADDING <const> = 28

local tiles <const> = {
	{ cursor = "default", color = 0xff39445f, hover = 0xff536181 },
	{ cursor = "arrow", color = 0xff3f4b6b, hover = 0xff5a6b95 },
	{ cursor = "ibeam", color = 0xff46506d, hover = 0xff64739c },
	{ cursor = "crosshair", color = 0xff40546b, hover = 0xff5c7895 },
	{ cursor = "pointing_hand", color = 0xff3e5960, hover = 0xff5a808a },
	{ cursor = "resize_ew", color = 0xff4f5364, hover = 0xff74798f },
	{ cursor = "resize_ns", color = 0xff4a5863, hover = 0xff6c808f },
	{ cursor = "resize_nwse", color = 0xff4b5064, hover = 0xff6f7692 },
	{ cursor = "resize_nesw", color = 0xff46556a, hover = 0xff667d9b },
	{ cursor = "resize_all", color = 0xff4f5961, hover = 0xff74828e },
	{ cursor = "not_allowed", color = 0xff5c4556, hover = 0xff8a657f },
}

for _, tile in ipairs(tiles) do
	tile.fill = quad.quad(TILE_W, TILE_H, tile.color)
	tile.hover_fill = quad.quad(TILE_W, TILE_H, tile.hover)
	tile.outline = quad.quad(TILE_W + 4, TILE_H + 4, 0xff8fb2ff)
end

soluna.set_window_title "soluna mouse cursor test"

local callback = {}
local mouse_x = -1
local mouse_y = -1
local active_cursor

local function columns()
	return math.max(1, math.floor((args.width - PADDING * 2 + GAP) / (TILE_W + GAP)))
end

local function tile_rect(index)
	local cols = columns()
	local col = (index - 1) % cols
	local row = math.floor((index - 1) / cols)
	return PADDING + col * (TILE_W + GAP), PADDING + row * (TILE_H + GAP)
end

local function contains(x, y, left, top)
	return x >= left and x < left + TILE_W and y >= top and y < top + TILE_H
end

local function cursor_at(x, y)
	for index, tile in ipairs(tiles) do
		local left, top = tile_rect(index)
		if contains(x, y, left, top) then
			return tile.cursor
		end
	end
end

local function set_cursor(cursor)
	if cursor == active_cursor then
		return
	end
	active_cursor = cursor
	soluna.set_mouse_cursor(cursor)
end

function callback.mouse_move(x, y)
	mouse_x, mouse_y = x, y
	set_cursor(cursor_at(x, y))
end

function callback.key(keycode, state)
	if keycode == KEY_ESCAPE and state == KEYSTATE_PRESS then
		app.quit()
	end
end

function callback.frame()
	for index, tile in ipairs(tiles) do
		local left, top = tile_rect(index)
		local hovered = contains(mouse_x, mouse_y, left, top)
		if hovered then
			batch:add(tile.outline, left - 2, top - 2)
		end
		batch:add(hovered and tile.hover_fill or tile.fill, left, top)
	end
end

return callback
