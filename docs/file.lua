---@meta soluna.file

---
--- Soluna file module
---

---@class soluna.file
local file = {}

---
--- Loads a file's contents
---
---@param filename string File path
---@return string? content File contents or nil on error
function file.load(filename) end

---
--- Gets file attributes
---
---@param filename string File path
---@return table? attributes File attributes or nil
function file.attributes(filename) end

---
--- Checks if a file exists
---
---@param filename string File path
---@return boolean? exists True if file exists, nil if not
function file.exist(filename) end

---
--- Checks if a local file exists
---
---@param filename string File path
---@return boolean? exists True if file exists, nil if not
function file.local_exist(filename) end

---
--- Loads a local file's contents
---
---@param filename string File path
---@return string? content File contents or nil
function file.local_load(filename) end

---
--- Iterates over directory entries
---
---@param path string Directory path
---@return function iterator Iterator function
function file.dir(path) end

return file
