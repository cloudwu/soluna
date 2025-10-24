---@meta soluna.font

---
--- Soluna font module
---

---@class soluna.font
local font = {}

---
--- Imports a TrueType font
---
---@param data string Raw TTF font data
function font.import(data) end

---
--- Gets font ID by name
---
---@param name string Font name (empty string for last imported font)
---@return integer fontid Font ID
function font.name(name) end

---
--- Gets the font system C object
---
---@return userdata fontcobj Font system object
function font.cobj() end

return font
