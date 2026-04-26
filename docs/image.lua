---@meta soluna.image

---图片模块
---Image module.
---@class soluna.image
local image = {}

---从 PNG 数据加载 RGBA 图片
---Loads RGBA image data from PNG bytes.
---@param data string PNG 数据 / PNG bytes
---@return string? data RGBA 像素数据，失败时为 nil / RGBA pixels, nil on failure
---@return integer|string width_or_error 成功时为宽度，失败时为错误信息 / Width on success, error message on failure
---@return integer? height 高度 / Height
function image.load(data)
end

---按比例缩放 RGBA 或灰度图片
---Resizes RGBA or grayscale image data by scale factors.
---@param data string RGBA 或灰度像素数据 / RGBA or grayscale pixels
---@param width integer 原始宽度 / Source width
---@param height integer 原始高度 / Source height
---@param scale_x number X 缩放倍率 / X scale factor
---@param scale_y? number Y 缩放倍率，默认等于 `scale_x` / Y scale factor, default is `scale_x`
---@return string data 缩放后的像素数据 / Resized pixels
---@return integer width 新宽度 / New width
---@return integer height 新高度 / New height
function image.resize(data, width, height, scale_x, scale_y)
end

return image
