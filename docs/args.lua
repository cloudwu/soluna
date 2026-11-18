---@meta

---可提交给 batch 的绘制对象
---Drawable object accepted by `Batch:add`.
---@alias soluna.Drawable integer|string|userdata

---绘制批次
---Render batch object.
---@class Batch
local batch = {}

---向批次添加 sprite、material 对象或 packed stream
---Adds a sprite id, material userdata, or packed command stream.
---@param sprite soluna.Drawable sprite ID、material userdata 或 packed string / Sprite id, material userdata, or packed string
---@param x? number X 坐标，默认 0 / X position, default 0
---@param y? number Y 坐标，默认 0 / Y position, default 0
function batch:add(sprite, x, y)
end

---打开或关闭变换层
---Opens or closes a transform layer.
---@overload fun(self: Batch)
---@overload fun(self: Batch, rotation: number)
---@overload fun(self: Batch, x: number, y: number)
---@overload fun(self: Batch, scale: number, x: number, y: number)
---@param scale number 缩放倍率，不能为 0 / Scale factor, cannot be 0
---@param rotation number 旋转弧度 / Rotation in radians
---@param x number X 平移 / X translation
---@param y number Y 平移 / Y translation
function batch:layer(scale, rotation, x, y)
end

---把屏幕点转换到当前 layer 坐标
---Transforms a screen point into the current layer space.
---@param x number 屏幕 X / Screen X
---@param y number 屏幕 Y / Screen Y
---@return number x 转换后的 X / Transformed X
---@return number y 转换后的 Y / Transformed Y
function batch:point(x, y)
end

---将后续绘制命令提交到指定的渲染通道。每帧开始时为屏幕（通道 1）。
---离屏通道按提交顺序执行，屏幕的所有命令在离屏通道之后合成一个通道绘制；
---同一帧内再次进入的离屏通道保留已有内容。
---Submits subsequent draw commands to the given render pass. Each frame starts
---with the screen (pass 1). Offscreen passes execute in submission order; all
---screen commands are drawn afterwards in a single pass. An offscreen pass
---entered again in the same frame keeps its contents.
---@param id integer 通道 id，屏幕为 1，渲染目标的通道由 `soluna.target` 返回 / Pass id: 1 for the screen, render target passes are returned by `soluna.target`
function batch:pass(id)
end

---入口参数表
---Entry argument table passed to the game script.
---@class Args
---@field width integer 当前窗口宽度 / Current window width
---@field height integer 当前窗口高度 / Current window height
---@field batch Batch 绘制批次 / Render batch
---@field [integer] string 启动参数 / Startup argument
local args = {}

return args
