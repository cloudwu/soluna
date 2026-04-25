---@meta soluna.font

---字体模块
---Font module.
---@class soluna.font
local font = {}

---导入 TrueType 字体数据
---Imports TrueType font data.
---@param data string TTF/TTC 字体数据 / TTF/TTC font data
function font.import(data)
end

---按字体族名获取 font id
---Gets a font id by family name.
---@param name string 字体族名 / Font family name
---@return integer? fontid 字体 id；找不到时为 nil / Font id, nil when not found
function font.name(name)
end

---返回字体管理器 C 指针
---Returns the native font manager pointer.
---@return lightuserdata fontcobj 字体管理器指针 / Font manager pointer
function font.cobj()
end

return font
