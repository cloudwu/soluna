---@meta soluna.material.mask

---
--- Soluna material mask module
---

---@class soluna.material.mask
local matmask = {}


--- Creates a colored mask sprite.
---
--- Color format: 0xAARRGGBB (native-endian 32-bit integer).
--- If the alpha byte is 00 it is forced to 0xFF (fully opaque).
---
---@param sprite integer 1-based sprite index (will be stored as 0-based internally)
---@param color  integer Color in 0xAARRGGBB format (e.g. 0xFF00FF00 for opaque green)
---@return userdata mask Binary blob containing the packed mask_primitive; push it as a Lua string
function matmask.mask(sprite, color) end

return matmask
