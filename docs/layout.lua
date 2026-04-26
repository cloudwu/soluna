---@meta soluna.layout

---layout 元素对象
---Layout element object.
---@class soluna.layout.Element
local element = {}

---更新元素 Yoga 属性
---Updates Yoga attributes on the element.
---@param attr table 属性表 / Attribute table
function element:update(attr)
end

---读取元素布局结果
---Reads calculated element layout.
---@return number x X 坐标 / X coordinate
---@return number y Y 坐标 / Y coordinate
---@return number w 宽度 / Width
---@return number h 高度 / Height
function element:get()
end

---返回元素属性表
---Returns the element attribute table.
---@return table attrs 属性表 / Attribute table
function element:attribs()
end

---layout 文档对象
---Layout document object.
---@class soluna.layout.Document
---@field [string] soluna.layout.Element 按 id 访问元素 / Element access by id

---layout 绘制条目
---Calculated drawable layout item.
---@class soluna.layout.Item
---@field x number X 坐标 / X coordinate
---@field y number Y 坐标 / Y coordinate
---@field w number 宽度 / Width
---@field h number 高度 / Height
---@field [string] any datalist 属性 / Datalist attributes

---layout 计算结果
---Calculated layout item list.
---@class soluna.layout.Result
---@field [integer] soluna.layout.Item 绘制条目 / Drawable item
---@field width number 根节点宽度 / Root width
---@field height number 根节点高度 / Root height

---Yoga layout 模块
---Yoga layout module.
---@class soluna.layout
local layout = {}

---加载 layout 定义
---Loads a layout definition.
---@param filename_or_list string|table layout 文件路径或已解析 datalist / Layout file path or parsed datalist
---@param scripts? fun(name: string): table children 动态 children resolver / Dynamic children resolver
---@return soluna.layout.Document document layout 文档 / Layout document
function layout.load(filename_or_list, scripts)
end

---计算 layout 并返回绘制条目
---Calculates layout and returns drawable items.
---@param document soluna.layout.Document layout 文档 / Layout document
---@return soluna.layout.Result items 绘制条目列表 / Drawable item list
function layout.calc(document)
end

return layout
