---@meta soluna.material.quad

---
--- Soluna material quad module
---

---@class soluna.material.quad
local matquad = {}

---
--- Creates a colored rectangle sprite
---
--- Color format: RGBA as 32-bit integer 0xRRGGBBAA.
--- If alpha channel (high byte) is 0, it defaults to 0xFF (opaque).
---
---@param width integer Rectangle width in pixels
---@param height integer Rectangle height in pixels
---@param color integer Color in RGBA format (0xRRGGBBAA, e.g., 0xFF0000FF for opaque red)
---@return userdata sprite Sprite object for rendering with batch:add()
function matquad.quad(width, height, color) end

return matquad
