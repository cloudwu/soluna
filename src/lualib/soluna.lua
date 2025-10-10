local ltask = require "ltask"
local app = require "soluna.app"
local mqueue = require "ltask.mqueue"

global require, error, string

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
	else
		error "TODO: support none windows"
	end
	path = path .. name
	return recursion_mkdir(lfs.personaldir() , path)
end

function soluna.load_sprites(filename)
	local loader = ltask.uniqueservice "loader"
	local sprites = ltask.call(loader, "loadbundle", filename)
	local render = ltask.uniqueservice "render"
	ltask.call(render, "load_sprites", filename)
	return sprites
end

local function version()
	local api, hash = app.version()
	return string.format("%03x", api) .. hash:sub(1, 7)
end

soluna.version = version()

return soluna
