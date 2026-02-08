-- To run this sample:
-- bin/soluna.exe entry=test/ime.lua

local soluna = require "soluna"
local app = require "soluna.app"
local mattext = require "soluna.material.text"
local matquad = require "soluna.material.quad"
local matmask = require "soluna.material.mask"
local font = require "soluna.font"
local file = require "soluna.file"
local utf8 = utf8
local math = math
local string = string
local table = table

local args = ...
local batch = assert(args.batch)

local KEY_LEFT <const> = 263
local KEY_RIGHT <const> = 262
local KEY_HOME <const> = 268
local KEY_END <const> = 269
local KEY_BACKSPACE <const> = 259
local KEY_DEL <const> = 261
local KEY_ENTER <const> = 257
local KEYSTATE_PRESS <const> = 1
local CHAR_BACKSPACE <const> = 8
local CHAR_DELETE <const> = 127

local FONT_SIZE <const> = 32
local HELP_SIZE <const> = 18
local BOX_WIDTH <const> = 720
local BOX_HEIGHT <const> = 84
local BOX_PADDING_X <const> = 18
local BOX_PADDING_Y <const> = 8
local BOX_RADIUS <const> = 10
local CURSOR_BLINK <const> = 30

local function load_font()
  if soluna.platform == "wasm" then
    local bundled_name = "Source Han Sans SC Regular"
    local bundled_path = "asset/font/SourceHanSansSC-Regular.ttf"
    local bundled_data = file.load(bundled_path)
    if bundled_data then
      font.import(bundled_data)
      local bundled_id = font.name(bundled_name)
      if bundled_id then
        return bundled_id, bundled_name
      end
    end
  end

  local sysfont = require "soluna.font.system"
  local candidates = {
    "WenQuanYi Micro Hei",        -- Linux
    "Microsoft YaHei",            -- Windows
    "Yuanti SC",                  -- macOS
    "Source Han Sans SC Regular", -- WASM
  }
  for _, name in ipairs(candidates) do
    local ok, data = pcall(sysfont.ttfdata, name)
    if ok and data then
      font.import(data)
      local fontid = font.name(name)
      if fontid then
        return fontid, name
      end
    end
  end
  error("No available system font for IME sample")
end

local function cache(f)
  return setmetatable({}, {
    __index = function(self, k)
      local v = f(k)
      self[k] = v
      return v
    end
  })
end

local quad_cache = cache(function(key)
  local w, h, c = key:match("^(%-?%d+):(%-?%d+):(%x+)$")
  return matquad.quad(tonumber(w), tonumber(h), tonumber(c, 16))
end)

local function cached_quad(w, h, c)
  local key = string.format("%d:%d:%08x", w, h, c)
  return quad_cache[key]
end

local mask_cache = cache(function(key)
  local sprite, color = key:match("^(%d+):(%x+)$")
  return matmask.mask(tonumber(sprite), tonumber(color, 16))
end)

local function cached_mask(sprite, color)
  local key = string.format("%d:%08x", sprite, color)
  return mask_cache[key]
end

local function clamp(v, lo, hi)
  if v < lo then
    return lo
  elseif v > hi then
    return hi
  end
  return v
end

local function rounded_box_rgba(w, h, radius)
  local r = math.floor(clamp(radius, 0, math.min(w, h) * 0.5))
  local edge = r * r
  local left = r
  local right = w - r
  local top = r
  local bottom = h - r
  local opaque = "\255\255\255\255"
  local transparent = "\255\255\255\0"
  local lines = {}
  for y = 0, h - 1 do
    local py = y + 0.5
    local row = {}
    for x = 0, w - 1 do
      local px = x + 0.5
      local qx = clamp(px, left, right)
      local qy = clamp(py, top, bottom)
      local dx = px - qx
      local dy = py - qy
      row[x + 1] = (dx * dx + dy * dy <= edge) and opaque or transparent
    end
    lines[y + 1] = table.concat(row)
  end
  return table.concat(lines)
end

local rounded_box_cache = {}

local function rounded_box_sprite(w, h, radius)
  local key = string.format("%d:%d:%d", w, h, radius)
  local sprite = rounded_box_cache[key]
  if sprite then
    return sprite
  end
  local filename = "@" .. "ime_round_" .. key:gsub(":", "_")
  soluna.preload({
    filename = filename,
    content = rounded_box_rgba(w, h, radius),
    w = w,
    h = h,
  })
  local sprites = soluna.load_sprites({
    {
      name = "box",
      filename = filename,
      cw = w,
      ch = h,
      x = 0,
      y = 0,
    }
  })
  rounded_box_cache[key] = sprites.box
  return sprites.box
end

local fontid, font_name = load_font()
local fontcobj = font.cobj()
local text_block, text_cursor = mattext.block(fontcobj, fontid, FONT_SIZE, 0x000000, "LV")
local help_block = mattext.block(fontcobj, fontid, HELP_SIZE, 0x222222, "LV")

soluna.set_window_title("soluna ime sample")
app.set_ime_font(font_name, FONT_SIZE)

local state = {
  screen_w = args.width,
  screen_h = args.height,
  mouse_x = 0,
  mouse_y = 0,
  focused = true,
  caret_tick = 0,
  text = "",
  cursor = 0,
  suppress_control_char = nil,
}

local function char_count(s)
  return utf8.len(s) or 0
end

local function clamp_cursor()
  local n = char_count(state.text)
  if state.cursor < 0 then
    state.cursor = 0
  elseif state.cursor > n then
    state.cursor = n
  end
end

local function byte_offset_for_char(index_1based)
  return utf8.offset(state.text, index_1based) or (#state.text + 1)
end

local function insert_text(s)
  if not s or s == "" then
    return
  end
  local byte = byte_offset_for_char(state.cursor + 1)
  state.text = state.text:sub(1, byte - 1) .. s .. state.text:sub(byte)
  state.cursor = state.cursor + char_count(s)
end

local function delete_backward()
  if state.cursor <= 0 then
    return
  end
  local from = byte_offset_for_char(state.cursor)
  local to = byte_offset_for_char(state.cursor + 1)
  state.text = state.text:sub(1, from - 1) .. state.text:sub(to)
  state.cursor = state.cursor - 1
end

local function delete_forward()
  local n = char_count(state.text)
  if state.cursor >= n then
    return
  end
  local from = byte_offset_for_char(state.cursor + 1)
  local to = byte_offset_for_char(state.cursor + 2)
  state.text = state.text:sub(1, from - 1) .. state.text:sub(to)
end

local function is_control_char(codepoint)
  return codepoint < 32 or (codepoint >= 127 and codepoint <= 159)
end

local function handle_control_delete(codepoint)
  if codepoint ~= CHAR_BACKSPACE and codepoint ~= CHAR_DELETE then
    return
  end
  if state.suppress_control_char == codepoint then
    state.suppress_control_char = nil
    return
  end
  state.suppress_control_char = nil
  if codepoint == CHAR_BACKSPACE then
    delete_backward()
  else
    delete_forward()
  end
end

local function decode_char_event(value)
  local t = type(value)
  if t == "number" then
    if is_control_char(value) then
      return nil, value
    end
    return utf8.char(value), nil
  end
  if t ~= "string" or value == "" then
    return nil, nil
  end
  local first = utf8.codepoint(value, 1, 1)
  if first and is_control_char(first) then
    return nil, first
  end
  return value, nil
end

local function box_rect()
  local w = math.min(BOX_WIDTH, math.max(320, state.screen_w - 48))
  local h = BOX_HEIGHT
  local x = (state.screen_w - w) // 2
  local y = (state.screen_h - h) // 2
  return x, y, w, h
end

local function in_box(x, y, bx, by, bw, bh)
  return x >= bx and x <= bx + bw and y >= by and y <= by + bh
end

local callback = {}

function callback.window_resize(w, h)
  state.screen_w = w
  state.screen_h = h
end

function callback.mouse_move(x, y)
  state.mouse_x = x
  state.mouse_y = y
end

function callback.mouse_button(button, key_state)
  if button ~= 0 or key_state ~= KEYSTATE_PRESS then
    return
  end
  local bx, by, bw, bh = box_rect()
  state.focused = in_box(state.mouse_x, state.mouse_y, bx, by, bw, bh)
  if not state.focused then
    app.set_ime_rect(nil)
  end
end

function callback.char(value)
  if not state.focused then
    return
  end
  local text_input, control = decode_char_event(value)
  if control then
    handle_control_delete(control)
    return
  end
  if not text_input then
    return
  end
  insert_text(text_input)
  clamp_cursor()
  state.caret_tick = 0
end

function callback.key(keycode, key_state)
  if key_state ~= KEYSTATE_PRESS or not state.focused then
    return
  end
  if keycode == KEY_LEFT then
    state.cursor = state.cursor - 1
  elseif keycode == KEY_RIGHT then
    state.cursor = state.cursor + 1
  elseif keycode == KEY_HOME then
    state.cursor = 0
  elseif keycode == KEY_END then
    state.cursor = char_count(state.text)
  elseif keycode == KEY_BACKSPACE then
    delete_backward()
    state.suppress_control_char = CHAR_BACKSPACE
  elseif keycode == KEY_DEL then
    delete_forward()
    state.suppress_control_char = CHAR_DELETE
  elseif keycode == KEY_ENTER then
    insert_text("\n")
  end
  clamp_cursor()
  state.caret_tick = 0
end

function callback.frame()
  clamp_cursor()
  local bx, by, bw, bh = box_rect()
  local box_sprite = rounded_box_sprite(bw, bh, BOX_RADIUS)
  local tx = bx + BOX_PADDING_X
  local ty = by + BOX_PADDING_Y
  local tw = bw - BOX_PADDING_X * 2
  local th = bh - BOX_PADDING_Y * 2

  batch:add(cached_quad(state.screen_w, state.screen_h, 0xf2f2f2ff), 0, 0)
  batch:add(cached_mask(box_sprite, state.focused and 0xffffffff or 0xe8e8e8ff), bx, by)
  batch:add(
    cached_quad(
      math.max(bw - BOX_RADIUS * 2, 2),
      2,
      state.focused and 0x1d6ef0ff or 0x9a9a9aff
    ),
    bx + BOX_RADIUS,
    by + bh - 2
  )

  local label = text_block(state.text, tw, th)
  batch:add(label, tx, ty)

  local cx, cy, cw, ch, n, descent = text_cursor(state.text, state.cursor, tw, th)
  state.cursor = n
  descent = descent or 0
  if state.focused then
    app.set_ime_rect {
      x = tx + cx,
      y = ty + cy - descent,
      width = cw,
      height = ch,
      text_color = 0xff000000,
    }
  else
    app.set_ime_rect(nil)
  end

  state.caret_tick = (state.caret_tick + 1) % (CURSOR_BLINK * 2)
  if state.focused and state.caret_tick < CURSOR_BLINK then
    batch:add(cached_quad(math.max(cw, 2), ch, 0x111111ff), tx + cx, ty + cy)
  end

  local help = help_block("Click box, type with CJK.", state.screen_w - 32, 24)
  batch:add(help, 16, 16)
end

return callback
