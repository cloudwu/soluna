---@meta soluna.crypt

---
--- Soluna crypt module
---

---@class soluna.crypt
local crypt = {}

---
--- Encodes binary data to hexadecimal string
---
--- Converts binary data to a hexadecimal string representation (lowercase).
---
---@param data string Binary data to encode
---@return string hex Hexadecimal string (lowercase)
function crypt.hexencode(data) end

---
--- Computes SHA-1 hash of input data
---
--- Returns the SHA-1 hash as binary data (20 bytes).
---
---@param data string Input data to hash
---@return string hash Binary SHA-1 hash (20 bytes)
function crypt.sha1(data) end

return crypt
