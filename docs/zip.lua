---@meta soluna.zip

---ZIP 模块
---ZIP module.
---@class soluna.zip
local zip = {}

---打开 ZIP 文件
---Opens a ZIP archive.
---@param filename string ZIP 文件路径 / ZIP file path
---@param mode "r"|"w"|"a" 打开模式：读、写、追加 / Open mode: read, write, append
---@return userdata? zipfile ZIP 句柄；失败时为 nil / ZIP handle, nil on failure
function zip.open(filename, mode)
end

return zip
