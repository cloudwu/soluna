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

--- Audio playback options
---
--- Values provided here override defaults from `sounds.dl`.
--- `stream = true` is intended for long-running audio such as background music.
---
---@class soluna.AudioPlayOptions
---@field group? string Audio bus name
---@field volume? number Linear volume multiplier
---@field pan? number Stereo pan in range `[-1.0, 1.0]`
---@field pitch? number Pitch multiplier
---@field loop? boolean Whether playback should loop
---@field stream? boolean Whether to stream instead of fully decoding up front

--- Audio voice handle returned by `soluna.play_sound()`
---
--- Represents one active playback instance.
--- When a voice finishes naturally, `:playing()` returns `false`.
--- Mutating methods return `false` when the voice is no longer valid.
---
---@class soluna.AudioVoice
local AudioVoice = {}

---
--- Stops playback
---
---@param fade_seconds? number Optional fade-out duration in seconds
---@return boolean ok False when the voice is no longer valid
function AudioVoice:stop(fade_seconds) end

---
--- Checks whether the voice is still playing
---
---@return boolean playing
function AudioVoice:playing() end

---
--- Sets voice volume
---
---@param volume number Linear volume multiplier
---@return boolean ok False when the voice is no longer valid
function AudioVoice:set_volume(volume) end

---
--- Sets voice pan
---
---@param pan number Stereo pan in range `[-1.0, 1.0]`
---@return boolean ok False when the voice is no longer valid
function AudioVoice:set_pan(pan) end

---
--- Sets voice pitch
---
---@param pitch number Pitch multiplier
---@return boolean ok False when the voice is no longer valid
function AudioVoice:set_pitch(pitch) end

---
--- Enables or disables looping
---
---@param loop boolean
---@return boolean ok False when the voice is no longer valid
function AudioVoice:set_loop(loop) end

---
--- Seeks to a playback position in seconds
---
---@param seconds number Target playback time in seconds
---@return boolean ok False when the voice is no longer valid
function AudioVoice:seek(seconds) end

---
--- Returns the current playback position in seconds
---
---@return number? seconds Current playback time
---@return string? err Error message when the voice is no longer valid
function AudioVoice:tell() end

--- Audio bus handle returned by `soluna.audio_bus()`
---
---@class soluna.AudioBus
local AudioBus = {}

---
--- Sets bus volume
---
---@param volume number Linear volume multiplier
---@return boolean ok False when the bus name is invalid
function AudioBus:set_volume(volume) end

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

---
--- Loads audio definitions from a datalist file
---
--- Each entry in `sounds.dl` must define:
--- - `name`
--- - `filename`
---
--- Optional entry fields:
--- - `group` (defaults to `"sound"`)
--- - `volume`
--- - `pan`
--- - `pitch`
--- - `loop`
--- - `stream`
---
--- `load_sounds()` may be called multiple times. Each bundle registers more sound definitions.
--- Re-loading the same bundle is ignored. Sound names must stay unique across loaded bundles.
--- Audio buses are created from the `group` field of loaded sound entries and reused by name.
---
---@param filename string Path to audio definition file (.dl format)
function soluna.load_sounds(filename) end

---
--- Plays a sound and returns a voice handle
---
--- The returned handle represents the active playback instance, not the sound definition.
--- Background music uses the same API; a typical music entry sets `group = "music"`,
--- `loop = true`, and `stream = true` in `sounds.dl`.
---
---@param name string Sound definition name from `sounds.dl`
---@param opts? soluna.AudioPlayOptions Per-playback option overrides
---@return soluna.AudioVoice? voice Active playback handle
---@return string? err Error message on failure
function soluna.play_sound(name, opts) end

---
--- Returns an audio bus handle
---
--- Buses are created from the `group` field in loaded `sounds.dl` entries.
--- When a sound entry omits `group`, it uses the default bus name `"sound"`.
---
---@param name string Audio bus name
---@return soluna.AudioBus? bus Bus handle
---@return string? err Error message when the bus does not exist
function soluna.audio_bus(name) end

return soluna
