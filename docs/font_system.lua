---@meta soluna.font.system

---系统字体模块
---System font module.
---@class soluna.font.system
local font_system = {}

---按字体族名读取系统 TTF/TTC 数据
---Reads system TTF/TTC data by family name.
---@param name string 字体族名 / Font family name
---@return string? data 字体数据；wasm 或失败时可能为 nil / Font data, nil on wasm or failure
function font_system.ttfdata(name)
end

return font_system
