---@meta soluna.lfs

---
--- Soluna lfs module
---

---@class soluna.lfs
local lfs = {}

---
--- Gets file attributes
---
---@param filename string File path
---@return table? attributes File attributes or nil
function lfs.attributes(filename) end

---
--- Iterates over directory entries
---
---@param path string Directory path
---@return function iterator Iterator function
function lfs.dir(path) end

return lfs
