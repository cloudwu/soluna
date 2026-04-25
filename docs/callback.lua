---@meta

---游戏入口返回的 callback 表
---Callback table returned by the game entry script.
---@class Callback
local callback = {}

---每帧调用
---Called once per frame.
---@param count integer frame 计数 / Frame counter
function callback.frame(count)
end

---键盘事件
---Keyboard event.
---@param keycode integer Sokol key code / Sokol key code
---@param state integer 1 为按下，0 为释放 / 1 for key down, 0 for key up
function callback.key(keycode, state)
end

---字符输入事件
---Text input event.
---@param codepoint integer Unicode codepoint / Unicode codepoint
function callback.char(codepoint)
end

---鼠标按钮事件
---Mouse button event.
---@param button integer 0 左键，1 右键，2 中键 / 0 left, 1 right, 2 middle
---@param state integer 1 为按下，0 为释放 / 1 for down, 0 for up
function callback.mouse_button(button, state)
end

---鼠标移动事件
---Mouse move event.
---@param x integer 逻辑像素 X / Logical pixel X
---@param y integer 逻辑像素 Y / Logical pixel Y
function callback.mouse_move(x, y)
end

---鼠标滚轮事件
---Mouse scroll event.
---@param y integer 垂直滚动量 / Vertical scroll delta
---@param x integer 水平滚动量 / Horizontal scroll delta
function callback.mouse_scroll(y, x)
end

---其它鼠标事件
---Other mouse event.
---@param event_type integer Sokol event type / Sokol event type
function callback.mouse(event_type)
end

---触摸开始
---Touch begin event.
---@param x integer 逻辑像素 X / Logical pixel X
---@param y integer 逻辑像素 Y / Logical pixel Y
function callback.touch_begin(x, y)
end

---触摸移动
---Touch move event.
---@param x integer 逻辑像素 X / Logical pixel X
---@param y integer 逻辑像素 Y / Logical pixel Y
function callback.touch_moved(x, y)
end

---触摸结束
---Touch end event.
---@param x integer 逻辑像素 X / Logical pixel X
---@param y integer 逻辑像素 Y / Logical pixel Y
function callback.touch_end(x, y)
end

---触摸取消
---Touch cancelled event.
---@param x integer 逻辑像素 X / Logical pixel X
---@param y integer 逻辑像素 Y / Logical pixel Y
function callback.touch_cancelled(x, y)
end

---窗口尺寸变化
---Window resize event.
---@param width integer 新窗口宽度 / New window width
---@param height integer 新窗口高度 / New window height
function callback.window_resize(width, height)
end

return callback
