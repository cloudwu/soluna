local ltask = require "ltask"
local audio = require "soluna.audio"
local file = require "soluna.file"
local datalist = require "soluna.datalist"
local soluna = require "soluna"

global print, assert, setmetatable, tostring, error, ipairs

local DEVICE, BANK, MAP

local api = {}

local play = audio.play

-- play
api[true] = function(id)
	play(DEVICE, BANK[id])
end

local S = {}

global type, tonumber, error, assert, ipairs, print, pairs

for k in pairs(api) do
	S[k] = function()
		error "Init audio first"
	end
end

local function load_bundle(filename)
	local b = datalist.parse(file.load(filename))
	local bank = {}
	local map = {}
	for i, v in ipairs(b) do
		bank[i] = assert(v.filename)
		map[assert(v.name)] = i
	end
	return bank, map
end

local ziplist

function S.init_device(device)
	ziplist = file.ziplist and file.ziplist()
	if ziplist then
		audio.init_vfs(device, ziplist)
	end
	DEVICE = device
end

function S.init(filename)
	assert(BANK == nil)
	BANK, MAP = load_bundle(filename)
	-- todo : preload file list
	local inject = ltask.dispatch()
	for k, v in pairs(api) do
		inject[k] = v
	end
end

function S.fetch()
	return MAP or "Init audio file list first"
end

function S.quit()
	if DEVICE then
		audio.deinit(DEVICE)
	end
	DEVICE = nil
end

return S
