---@meta soluna.layout

---
--- Soluna layout module
---

---@class soluna.layout
local layout = {}

---
--- Loads a layout definition from a file or table
---
--- The filename_or_list parameter can be:
--- - A string: path to a .dl layout file (will be loaded and parsed)
--- - A table: pre-parsed datalist structure
---
--- The scripts parameter is optional and provides a function table for script resolution.
---
---@param filename_or_list string|table Layout definition file path or parsed list
---@param scripts? table Script resolver function table
---@return table document Layout document object with element access by ID
function layout.load(filename_or_list, scripts) end

---
--- Calculates layout positions and dimensions
---
--- Runs the Yoga layout calculation on the document and updates all element positions.
--- Returns an array of element objects, each with x, y, w, h fields set to calculated values.
---
---@param document table Layout document from layout.load()
---@return table[] elements Array of element objects with calculated x, y, w, h positions
function layout.calc(document) end

return layout
