---@meta soluna.text

---文本预处理模块
---Text preprocessing module.
---@class soluna.text
---@field convert table<string, string> 文本转换缓存表 / Text conversion cache table
local text = {}

---初始化内嵌 icon bundle
---Initializes the embedded icon bundle.
---@param bundle_file string icon bundle `.dl` 文件路径 / Icon bundle `.dl` path
function text.init(bundle_file)
end

return text
