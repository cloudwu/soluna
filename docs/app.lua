---@meta soluna.app

---输入法候选窗口矩形
---IME candidate window rectangle.
---@class soluna.app.ImeRect
---@field x number 左上角 X / Left coordinate
---@field y number 左上角 Y / Top coordinate
---@field width number 宽度 / Width
---@field height number 高度 / Height
---@field text_color? integer 文本 ARGB 颜色；alpha 为 0 时补为 0xff / Text color in ARGB; alpha 0 is promoted to 0xff

---应用控制模块
---Application control module.
---@class soluna.app
local app = {}

---请求应用优雅退出
---Requests graceful application quit.
function app.quit()
end

---设置输入法字体
---Sets the IME font face and pixel size.
---@overload fun()
---@overload fun(font_size: number)
---@param font_name? string 字体名；nil 表示平台默认字体 / Font face; nil uses platform default
---@param font_size number 字体像素大小 / Font size in pixels
function app.set_ime_font(font_name, font_size)
end

---设置输入法候选窗口矩形
---Sets the IME candidate window rectangle.
---@param rect? soluna.app.ImeRect nil 会清除矩形 / nil clears the rectangle
function app.set_ime_rect(rect)
end

return app
