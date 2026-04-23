local ltask = require "ltask"
local app = require "soluna.app"
local mqueue = require "ltask.mqueue"

global require, error, string, assert, package, setmetatable, tostring

local soluna = {
	platform = app.platform
}

function soluna.gamepad_init()
	local gamepad = require "soluna.gamepad"
	local state = {}
	soluna.gamepad = state
	local gs = ltask.uniqueservice "gamepad"
	local S = ltask.dispatch()

	function S._gamepad_update()
		gamepad.update(state)
	end

	ltask.call(gs, "register", ltask.self(), "_gamepad_update")

	return state
end

local settings
function soluna.settings()
	if settings == nil then
		local s = ltask.queryservice "settings"
		settings = ltask.call(s, "get")
	end
	return settings
end

function soluna.set_window_title(text)
	mqueue.send(app.mqueue(), ltask.pack("set_title", text))
end

function soluna.set_icon(data)
	mqueue.send(app.mqueue(), ltask.pack("set_icon", data))
end

local function recursion_mkdir(root, path)
	local lfs = require "soluna.lfs"
	for p in path:gmatch "[^/\\]+" do
		root = root .. "/" .. p
		lfs.mkdir(root)
	end
	return (root:gsub("[^/\\]$", "%0/"))
end

function soluna.gamedir(name)
	if name == nil then
		settings = settings and soluna.settings()
		name = settings.project or error "missing project name in settings"
	end
	local lfs = require "soluna.lfs"
	local path
	if soluna.platform == "windows" then
		path = "My Games/"
	elseif soluna.platform == "macos" or soluna.platform == "linux" then
		path = ".local/share/"
	elseif soluna.platform == "wasm" then
		path = "persistent/games/"
	else
		error "TODO: support none windows"
	end
	path = path .. name
	return recursion_mkdir(lfs.personaldir(), path)
end

function soluna.load_sprites(filename)
	local render = ltask.uniqueservice "render"
	local sprites = ltask.call(render, "load_sprites", filename)
	return sprites
end

local audio_service

local voice_index = {}
local voice_mt = { __index = voice_index }

function voice_index:stop(fade_seconds)
	return ltask.call(audio_service, "voice_stop", self.id, fade_seconds)
end

function voice_index:playing()
	return ltask.call(audio_service, "voice_playing", self.id)
end

function voice_index:set_volume(volume)
	return ltask.call(audio_service, "voice_set_volume", self.id, volume)
end

function voice_index:set_pan(pan)
	return ltask.call(audio_service, "voice_set_pan", self.id, pan)
end

function voice_index:set_pitch(pitch)
	return ltask.call(audio_service, "voice_set_pitch", self.id, pitch)
end

function voice_index:set_loop(loop)
	return ltask.call(audio_service, "voice_set_loop", self.id, loop)
end

function voice_index:seek(seconds)
	return ltask.call(audio_service, "voice_seek", self.id, seconds)
end

function voice_index:tell()
	return ltask.call(audio_service, "voice_tell", self.id)
end

local bus_index = {}
local bus_mt = { __index = bus_index }

function bus_index:set_volume(volume)
	return ltask.call(audio_service, "bus_set_volume", self.name, volume)
end

function soluna.load_sounds(filename)
	audio_service = audio_service or ltask.uniqueservice "audio"
	ltask.call(audio_service, "init", filename)
end

function soluna.play_sound(name, opts)
	local id, err = ltask.call(audio_service, "play_sound", name, opts)
	if not id then
		return nil, err
	end
	return setmetatable({ id = id }, voice_mt)
end

function soluna.audio_bus(name)
	if not ltask.call(audio_service, "has_bus", name) then
		return nil, "Unknown audio bus " .. tostring(name)
	end
	return setmetatable({ name = name }, bus_mt)
end

function soluna.preload(spr)
	local loader = ltask.uniqueservice "loader"
	if #spr == 0 then
		ltask.call(loader, "preload", spr.filename, spr.content, spr.w, spr.h)
	else
		local async = ltask.async()
		for i = 1, #spr do
			local s = spr[i]
			async:request(loader, "preload", s.filename, s.content, s.w, s.h)
		end
		async:wait()
	end
end

local function version()
	local api, hash = app.version()
	soluna.version_api = api
	return string.format("%03x", api) .. hash:sub(1, 7)
end

soluna.version = version()

return soluna
