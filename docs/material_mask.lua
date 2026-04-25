---@meta soluna.material.mask

---mask material 模块
---Mask material module.
---@class soluna.material.mask
local matmask = {}

---创建带颜色遮罩的 sprite command stream
---Creates a colored mask sprite command stream.
---@param sprite integer 1-based sprite id / 1-based sprite id
---@param color integer ARGB 颜色，alpha 为 0 时补为 0xff / ARGB color; alpha 0 is promoted to 0xff
---@return string stream 可传给 `batch:add` 的 packed stream / Packed stream for `batch:add`
function matmask.mask(sprite, color)
end

return matmask
