---@meta
---

---@class Batch
local batch = {}

---
--- Adds a sprite to the render batch
---
--- The sprite can be:
--- - A sprite ID (number) from a loaded sprite bundle
--- - A material userdata (from mattext.block or matquad.quad)
--- - A binary string of pre-packed vertex data (size must be multiple of 2×sizeof(draw_primitive))
---
---@param sprite number|userdata|string Sprite ID, material object, or command string
---@param x? number X position (default: 0)
---@param y? number Y position (default: 0)
function batch:add(sprite, x, y) end

---
--- Creates or closes a transformation layer
---
--- Layers apply scale, rotation, and translation transformations to all sprites
--- added while the layer is active. Layers can be nested.
---
--- Usage:
--- - batch:layer() with no args: closes the current layer
--- - batch:layer(rotation): applies rotation only (scale=1, x=0, y=0)
--- - batch:layer(x, y): applies translation only (scale=1, rotation=0)
--- - batch:layer(scale, x, y): applies scale and translation (rotation=0)
--- - batch:layer(scale, rotation, x, y): applies all transformations
---
---@overload fun(self: Batch)
---@overload fun(self: Batch, rotation: number)
---@overload fun(self: Batch, x: number, y: number)
---@overload fun(self: Batch, scale: number, x: number, y: number)
---@param scale number Scale factor (cannot be 0)
---@param rotation number Rotation in radians
---@param x number X translation
---@param y number Y translation
function batch:layer(scale, rotation, x, y) end

---@class Args
---@field width integer Current window width
---@field height integer Current window height
---@field batch Batch Render batch object
local args = {}

return args
