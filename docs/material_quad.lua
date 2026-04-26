---@meta soluna.material.quad

---quad material 模块
---Quad material module.
---@class soluna.material.quad
local matquad = {}

---创建纯色矩形 command stream
---Creates a solid rectangle command stream.
---@param width integer 宽度 / Width
---@param height integer 高度 / Height
---@param color integer ARGB 颜色，alpha 为 0 时补为 0xff / ARGB color; alpha 0 is promoted to 0xff
---@return string stream 可传给 `batch:add` 的 packed stream / Packed stream for `batch:add`
function matquad.quad(width, height, color)
end

return matquad
