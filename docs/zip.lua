---@meta soluna.zip

---
--- Soluna zip module
---

---@class soluna.zip
local zip = {}

---
--- Opens a ZIP file for reading or writing
---
--- Opens a ZIP archive. Mode "r" opens for reading, "w" creates a new archive.
--- Returns a file handle or nil on error.
---
---@param filename string Path to ZIP file
---@param mode "r"|"w" Open mode: "r" for read, "w" for write
---@return userdata? zipfile ZIP file handle or nil on error
function zip.open(filename, mode) end

return zip
