---@meta soluna.crypt

---密码辅助模块
---Cryptography helper module.
---@class soluna.crypt
local crypt = {}

---编码为小写十六进制字符串
---Encodes binary data as lower-case hex.
---@param data string 二进制数据 / Binary data
---@return string hex 十六进制字符串 / Hex string
function crypt.hexencode(data)
end

---计算 SHA-1 摘要
---Calculates SHA-1 digest.
---@param data string 输入数据 / Input data
---@return string hash 20 字节摘要 / 20-byte digest
function crypt.sha1(data)
end

return crypt
