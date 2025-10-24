---@meta soluna.datalist

---
--- Soluna datalist module
---

---@class soluna.datalist
local datalist = {}

---
--- Parses datalist format data
---
---@param data string Datalist format text
---@return table parsed Parsed data structure
function datalist.parse(data) end

---
--- Quotes a string for datalist format
---
---@param str string String to quote
---@return string quoted Quoted string
function datalist.quote(str) end

return datalist
