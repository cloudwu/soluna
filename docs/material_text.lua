---@meta soluna.material.text

---文本块创建函数
---Text block builder function.
---@alias soluna.material.text.Block fun(text: string, width?: integer, height?: integer): string, integer

---光标位置查询函数
---Text cursor query function.
---@alias soluna.material.text.Cursor fun(text: string, position: integer, width?: integer, height?: integer): integer, integer, integer, integer, integer, integer

---text material 模块
---Text material module.
---@class soluna.material.text
local mattext = {}

---创建文本块和光标查询函数
---Creates text block and cursor query functions.
---@param fontcobj lightuserdata `font.cobj()` 返回的字体管理器指针 / Font manager pointer returned by `font.cobj()`
---@param fontid integer `font.name()` 返回的字体 id / Font id returned by `font.name()`
---@param size? integer 字体像素大小，默认 16 / Font pixel size, default 16
---@param color? integer ARGB 颜色，默认 `0xff000000` / ARGB color, default `0xff000000`
---@param alignment? string 对齐代码，如 `"LT"`、`"CV"`、`"RB"` / Alignment code such as `"LT"`, `"CV"`, `"RB"`
---@return soluna.material.text.Block block 创建 packed text stream / Creates packed text stream
---@return soluna.material.text.Cursor cursor 查询光标矩形 / Queries cursor rectangle
function mattext.block(fontcobj, fontid, size, color, alignment)
end

return mattext
