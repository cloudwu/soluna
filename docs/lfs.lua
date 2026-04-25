---@meta soluna.lfs

---本地文件属性表
---Local file attribute table.
---@class soluna.lfs.Attributes
---@field mode "file"|"directory"|"link"|"socket"|"named pipe"|"char device"|"block device"|"other" 文件类型 / File type
---@field size integer 文件大小 / File size
---@field access integer 最后访问时间 / Last access time
---@field modification integer 最后修改时间 / Last modification time
---@field change integer 最后状态变化时间 / Last status change time
---@field permissions string 权限字符串 / Permission string

---本地文件系统模块
---Local filesystem module.
---@class soluna.lfs
local lfs = {}

---获取文件属性
---Gets file attributes.
---@param filename string 文件路径 / File path
---@param member? string 可选属性名 / Optional attribute name
---@return soluna.lfs.Attributes|integer|string|nil attributes 属性表或指定属性 / Attribute table or selected attribute
---@return string? err 错误信息 / Error message
---@return integer? errno 系统错误码 / System errno
function lfs.attributes(filename, member)
end

---遍历目录条目
---Iterates directory entries.
---@param path string 目录路径 / Directory path
---@return fun(): string? iterator 迭代器 / Iterator
---@return userdata state 目录句柄 / Directory handle
function lfs.dir(path)
end

return lfs
