---@meta soluna.file

---文件属性表
---File attribute table.
---@class soluna.file.Attributes
---@field mode "file"|"directory"|"link"|"socket"|"named pipe"|"char device"|"block device"|"other" 文件类型 / File type
---@field dev integer 设备号 / Device id
---@field ino integer inode / Inode
---@field nlink integer 硬链接数 / Hard link count
---@field uid integer owner user id / Owner user id
---@field gid integer owner group id / Owner group id
---@field rdev integer special file device id / Special file device id
---@field access integer 最后访问时间 / Last access time
---@field modification integer 最后修改时间 / Last modification time
---@field change integer 最后状态变化时间 / Last status change time
---@field size integer 文件大小 / File size
---@field permissions string 权限字符串 / Permission string

---文件加载模块
---File loading module.
---@class soluna.file
local file = {}

---加载文件内容
---Loads file contents.
---@param filename string 文件路径 / File path
---@param mode? string 本地文件打开模式，默认 `"rb"` / Local file open mode, default `"rb"`
---@return string? content 文件内容；失败返回 nil / File contents, nil on failure
function file.load(filename, mode)
end

---获取文件属性
---Gets file attributes.
---@param filename string 文件路径 / File path
---@return soluna.file.Attributes|string? attributes 本地文件返回属性表，zip 文件可返回 `"file"` 或 `"directory"` / Local files return attributes; zip files may return `"file"` or `"directory"`
function file.attributes(filename)
end

---判断文件是否存在
---Checks whether a file exists.
---@param filename string 文件路径 / File path
---@return boolean? exists 存在时为 true，否则为 nil / true when found, nil otherwise
function file.exist(filename)
end

---判断本地文件是否存在
---Checks whether a local file exists.
---@param filename string 文件路径 / File path
---@return boolean? exists 存在时为 true，否则为 nil / true when found, nil otherwise
function file.local_exist(filename)
end

---加载本地文件内容
---Loads local file contents.
---@param filename string 文件路径 / File path
---@param mode? string 打开模式，默认 `"rb"` / Open mode, default `"rb"`
---@return string? content 文件内容；失败返回 nil / File contents, nil on failure
function file.local_load(filename, mode)
end

---遍历目录条目
---Iterates directory entries.
---@param path string 目录路径 / Directory path
---@return fun(): string? iterator 迭代器 / Iterator
---@return userdata? state 本地目录句柄 / Local directory handle
function file.dir(path)
end

return file
