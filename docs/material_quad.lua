---@meta soluna.material.quad

---
--- Soluna material quad module
---

---@class soluna.material.quad
local matquad = {}

---
--- Creates a colored rectangle sprite
---
--- Color format: ARGB as 32-bit integer 0xAARRGGBB.
--- If alpha channel (high byte) is 0, it defaults to 0xFF (opaque).
---
---@param width integer Rectangle width in pixels
---@param height integer Rectangle height in pixels
---@param color integer Color in ARGB format (0xAARRGGBB, e.g., 0xFFFF0000 for opaque red)
---@return userdata sprite Sprite object for rendering with batch:add()
function matquad.quad(width, height, color) end

return matquad
