---@meta soluna.material.target

---渲染目标材质模块。
---Render target material module.
---
---`target(sprite)` 将后续绘制命令定向至指定的渲染目标，`target()` 恢复上一层目标。
---目标支持嵌套，最外层为屏幕。面向同一渲染目标的每段连续绘制命令组成独立的渲染通道
---（render pass）。离屏通道按命令顺序执行，屏幕通道在所有离屏通道结束后执行。
---渲染目标在每帧的首次渲染通道开始时清屏，同一帧内的后续通道保留已有内容。
---本帧未使用的渲染目标保留原有内容。
---渲染目标纹理采用预乘 alpha 格式存储颜色。
---
---`target(sprite)` directs subsequent draw commands to the specified render
---target; `target()` restores the previous target. Targets can be nested, with
---the screen at the outermost level. Each continuous sequence of draw commands
---for a render target forms a separate render pass. Offscreen passes execute
---in command order, followed by the screen pass. Each render target is cleared
---at the start of its first pass in a frame; subsequent passes in the same frame
---preserve its contents. Targets unused in a frame retain their existing contents.
---Render target textures store colors in premultiplied alpha format.
---@class soluna.material.target
local mattarget = {}

---切换渲染目标。
---Switches the render target.
---@overload fun(): string
---@param sprite integer `soluna.target` 返回的 sprite / Sprite returned by `soluna.target`.
---@return string stream 打包后的命令流 / Packed command stream.
function mattarget.target(sprite)
end

return mattarget
