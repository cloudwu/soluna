---@meta soluna.material.perspective_quad

---
--- Soluna material perspective quad module
---

---@class soluna.material.perspective_quad
local matproj = {}

---@class soluna.material.perspective_quad.options
---@field quad? number[] Local quad coordinates as {x0,y0,x1,y1,x2,y2,x3,y3}
---@field scale_x? number Local x scale (default: 1.0)
---@field scale_y? number Local y scale (default: 1.0)
---@field shear_x? number Local x shear (default: 0.0)
---@field shear_y? number Local y shear (default: 0.0)
---@field q? number[] Perspective factors as {q0,q1,q2,q3}, defaults to all 1.0
---@field color? integer Color tint in ARGB format (0xAARRGGBB), default: 0xffffffff

---
--- Creates a perspective quad sprite command stream.
---
--- The result should be passed to `batch:add(...)`.
--- The sprite index is 1-based, consistent with sprite bundle ids.
--- Use `quad` for custom local geometry; otherwise use default sprite geometry.
---
---@param sprite integer 1-based sprite id
---@param options soluna.material.perspective_quad.options Perspective quad options
---@return string stream Packed perspective quad stream (for `batch:add`)
function matproj.sprite(sprite, options) end

return matproj
