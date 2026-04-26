---@meta soluna.material.perspective_quad

---perspective quad 选项
---Perspective quad options.
---@class soluna.material.perspective_quad.Options
---@field quad? number[] 自定义四角坐标 `{x0,y0,x1,y1,x2,y2,x3,y3}` / Custom corner coordinates
---@field scale_x? number X 缩放，默认 1 / X scale, default 1
---@field scale_y? number Y 缩放，默认 1 / Y scale, default 1
---@field shear_x? number X shear，默认 0 / X shear, default 0
---@field shear_y? number Y shear，默认 0 / Y shear, default 0
---@field q? number[] 四角 perspective q，默认全 1 / Per-corner perspective q, default all 1
---@field color? integer ARGB 颜色，默认 `0xffffffff` / ARGB color, default `0xffffffff`

---perspective quad material 模块
---Perspective quad material module.
---@class soluna.material.perspective_quad
local matproj = {}

---创建 perspective sprite command stream
---Creates a perspective sprite command stream.
---@param sprite integer 1-based sprite id / 1-based sprite id
---@param options soluna.material.perspective_quad.Options 绘制选项 / Draw options
---@return string stream 可传给 `batch:add` 的 packed stream / Packed stream for `batch:add`
function matproj.sprite(sprite, options)
end

return matproj
