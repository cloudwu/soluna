local ltask = require "ltask"
local audio = require "soluna.audio"
local file = require "soluna.file"
local datalist = require "soluna.datalist"

global print, assert, setmetatable, tostring, error, ipairs

local DEVICE, BANK

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

local M = {}

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

function S.init(filename)
	assert(DEVICE == nil)
	DEVICE = false
	local bank, ret = load_bundle(filename)
	local d, err = audio.init()
	if err then
		print ("Error : ", err)
		return {}
	else
		-- todo : load file list
		BANK = bank
		DEVICE = d
		local inject = ltask.dispatch()
		for k, v in pairs(api) do
			inject[k] = v
		end
		return ret
	end
end

function S.quit()
	if DEVICE then
		audio.deinit(DEVICE)
		DEVICE = nil
	end
end

return S
