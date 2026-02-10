---@meta soluna.material.text

---
--- Soluna material text module
---

---@class soluna.material.text
local mattext = {}

---
--- Creates a text block renderer
---
--- Returns two functions:
--- 1. block(text, width, height) - creates a renderable text sprite
--- 2. cursor(text, position, width, height) - calculates cursor position in text
---
--- Color format: ARGB as 32-bit integer 0xAARRGGBB.
--- If alpha channel (high byte) is 0, defaults to 0xFF (opaque).
--- Alignment codes (case-insensitive, can be combined):
---   Horizontal: L (left), C (center), R (right)
---   Vertical: T (top), V (center), B (bottom)
---   Examples: "LT" (left-top), "CV" (center-vertical), "RB" (right-bottom)
---
---@param fontcobj userdata Font system C object from font.cobj()
---@param fontid integer Font ID from font.name()
---@param size? integer Font size in pixels (default: 16)
---@param color? integer Text color (ARGB as 0xAARRGGBB, default: 0xff000000 = opaque black)
---@param alignment? string Alignment code (default: no alignment)
---@return fun(text: string, width: integer, height: integer): userdata block_function Creates text sprite
---@return fun(text: string, position: integer, width: integer, height: integer): integer, integer, integer, integer, integer, integer cursor_function Returns x, y, w, h, actual_position, descent
function mattext.block(fontcobj, fontid, size, color, alignment) end

return mattext
