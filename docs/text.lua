---@meta soluna.text

---
--- Soluna text module
---

---@class soluna.text
local text = {}

---
--- Initializes the text system with an icon bundle
---
--- Loads an icon sprite bundle and makes it available for embedding in text via text.convert.
--- The bundle is kept in memory to prevent garbage collection.
---
---@param bundle_file string Path to icon sprite bundle file (.dl format)
function text.init(bundle_file) end

---
--- Table that converts text strings with embedded icon tags and color codes
---
--- Usage: local converted = text.convert[original_text]
---
--- Supports:
--- - Icon embedding: [icon_name] - replaced with icon from bundle loaded with text.init()
--- - Color codes: [RRGGBB] - sets text color (RGB hex, e.g., [FF0000] for red)
--- - Named colors: [red], [green], [blue], [white], [black], [aqua], [yellow], [pink], [gray]
--- - Custom hex colors: [cRRGGBB] - custom RGB (e.g., [c808080] for gray)
--- - Escape brackets: [[ - literal bracket (replaced with [bracket] internally)
---
--- The table uses weak keys/values for memory efficiency.
---
---@type table<string, string>
text.convert = {}

return text
