---@meta

---
--- Soluna Game Engine API Reference
---
--- This file documents the Soluna API using Lua meta annotations.
---

--- Sprite ID type for single sprites or animation frames
---@alias Sprite integer|integer[]

--- Sprite bundle mapping sprite names to IDs
---@alias SpriteBundle table<string, Sprite?>

---@class soluna
local soluna = {}

---
--- Current platform identifier
---
---@type "windows"|"macos"|"linux"|"wasm"
soluna.platform = "windows"

---
--- Version string
---
---@type string
soluna.version = ""

---
--- API version number
---
---@type number
soluna.version_api = 0

---
--- Returns the game settings table
---
---@return table settings Game configuration from .game file
function soluna.settings() end

---
--- Sets the window title
---
---@param text string The window title text
function soluna.set_window_title(text) end

---
--- Sets the window icon
---
---@param data table Array of icon data tables with {data=..., width=..., height=...}
function soluna.set_icon(data) end

---
--- Returns the game data directory path
---
---@param name? string Project name (defaults to settings.project)
---@return string path Absolute path to game data directory
function soluna.gamedir(name) end

---
--- Loads a sprite bundle from a file
---
---@param filename string Path to sprite definition file (.dl format)
---@return SpriteBundle sprites Sprite bundle mapping sprite names to IDs
function soluna.load_sprites(filename) end

return soluna
