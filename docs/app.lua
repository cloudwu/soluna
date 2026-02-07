---@meta soluna.app

---
--- Soluna app module
---

---@class soluna.app
local app = {}

---
--- Quits the application
---
--- Signals the application to exit gracefully.
---
function app.quit() end

---
--- Sets the IME (Input Method Editor) font
---
--- Configures the font used for IME text input display.
---
---@param font_name string Font name for IME display
---@param font_size integer Font size for IME display
function app.set_ime_font(font_name, font_size) end

---
--- Sets the IME (Input Method Editor) position rectangle
---
--- Defines the screen position where IME candidate window should appear.
--- Pass `nil` as first argument to clear IME rect.
--- `text_color` is optional ARGB color; if alpha is `0x00`, opaque alpha is implied.
---
---@param x integer|nil X position of IME rectangle, or nil to clear
---@param y? integer Y position of IME rectangle
---@param width? integer Width of IME rectangle
---@param height? integer Height of IME rectangle
---@param text_color? integer Optional ARGB text color
function app.set_ime_rect(x, y, width, height, text_color) end

return app
