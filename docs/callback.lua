---@meta
---

---@class Callback
local callback = {}

---
--- Called every frame
---
---@param count integer Frame number
function callback.frame(count) end

---
--- Called on keyboard events
---
---@param keycode integer Key code
---@param state integer 0=release, 1=press, 2=repeat
function callback.key(keycode, state) end

---
--- Called on character input events
---
---@param char string UTF-8 character
function callback.char(char) end

---
--- Called on mouse button events
---
---@param button integer 0=left, 1=right, 2=middle
---@param state integer 0=release, 1=press
function callback.mouse_button(button, state) end

---
--- Called on mouse movement
---
---@param x integer Mouse X position
---@param y integer Mouse Y position
function callback.mouse_move(x, y) end

---
--- Called on mouse wheel scroll
---
---@param dx number Horizontal scroll delta
---@param dy number Vertical scroll delta
function callback.mouse_scroll(dx, dy) end

---
--- Called on window resize
---
---@param width integer New window width
---@param height integer New window height
function callback.window_resize(width, height) end

return callback
