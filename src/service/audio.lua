local ltask = require "ltask"
local audio = require "soluna.audio"
local file = require "soluna.file"
local datalist = require "soluna.datalist"

global assert, error, ipairs, math, pairs, tonumber, tostring

local device
local definitions = {}
local groups = {}
local voices = {}
local bundles = {}
local next_voice_id_value = 0
local ziplist
local is_quit

local SOUND_FLAG_STREAM = 0x00000001
local DEFAULT_DEFINITION = {
	group = "sound",
	volume = 1.0,
	pan = 0.0,
	pitch = 1.0,
	loop = false,
	stream = false,
}

local function convert_value(v)
	if v == "true" then
		return true
	end
	if v == "false" then
		return false
	end
	return tonumber(v) or v
end

local function load_bundle(filename)
	local source = file.load(filename)
	local bundle = datalist.parse(assert(source, "Can't load audio bundle " .. tostring(filename)))
	local defs = {}
	for _, v in ipairs(bundle) do
		local def = {
			group = DEFAULT_DEFINITION.group,
			volume = DEFAULT_DEFINITION.volume,
			pan = DEFAULT_DEFINITION.pan,
			pitch = DEFAULT_DEFINITION.pitch,
			loop = DEFAULT_DEFINITION.loop,
			stream = DEFAULT_DEFINITION.stream,
		}
		for k, value in pairs(v) do
			def[k] = convert_value(value)
		end
		def.name = assert(def.name)
		def.filename = assert(def.filename)
		if defs[def.name] then
			error("Duplicate sound " .. tostring(def.name))
		end
		defs[def.name] = def
	end
	return defs
end

local function merge_definition(def, opts)
	if opts == nil then
		return {
			filename = def.filename,
			group = def.group,
			volume = def.volume,
			pan = def.pan,
			pitch = def.pitch,
			loop = def.loop,
			stream = def.stream,
		}
	end
	local loop = opts.loop
	if loop == nil then
		loop = def.loop
	end
	local stream = opts.stream
	if stream == nil then
		stream = def.stream
	end
	return {
		filename = def.filename,
		group = opts.group ~= nil and opts.group or def.group,
		volume = opts.volume ~= nil and opts.volume or def.volume,
		pan = opts.pan ~= nil and opts.pan or def.pan,
		pitch = opts.pitch ~= nil and opts.pitch or def.pitch,
		loop = loop,
		stream = stream,
	}
end

local function release_voice(id)
	local voice = voices[id]
	if not voice then
		return
	end
	audio.sound_uninit(voice)
	voices[id] = nil
end

local function cleanup_voices()
	for id, voice in pairs(voices) do
		if not audio.sound_playing(voice) then
			release_voice(id)
		end
	end
end

local function next_voice_id()
	next_voice_id_value = next_voice_id_value + 1
	return next_voice_id_value
end

local function seconds_to_ms(seconds)
	if seconds == nil then
		return nil
	end
	if seconds <= 0 then
		return 0
	end
	return math.floor(seconds * 1000 + 0.5)
end

ltask.fork(function()
	while not is_quit do
		cleanup_voices()
		ltask.sleep(20)
	end
end)

local S = {}

function S.init_device(dev)
	ziplist = file.ziplist and file.ziplist()
	if ziplist then
		audio.init_vfs(dev, ziplist)
	end
	device = dev
end

function S.init(filename)
	if bundles[filename] then
		return
	end
	local defs = load_bundle(filename)
	for name, def in pairs(defs) do
		if definitions[name] then
			error("Duplicate sound " .. tostring(name))
		end
		local group = groups[def.group]
		if group == nil then
			group = assert(audio.group_init(device))
			groups[def.group] = group
		end
		definitions[name] = def
	end
	bundles[filename] = true
end

function S.play_sound(name, opts)
	local def = definitions[name]
	if def == nil then
		return nil, "Unknown sound " .. tostring(name)
	end

	local final = merge_definition(def, opts)
	local group = groups[final.group]
	if group == nil then
		return nil, "Unknown audio bus " .. tostring(final.group)
	end

	local flags = final.stream and SOUND_FLAG_STREAM or 0

	local voice, err = audio.sound_init(device, final.filename, flags, group)
	if not voice then
		return nil, err
	end

	audio.sound_set_volume(voice, final.volume)
	audio.sound_set_pan(voice, final.pan)
	audio.sound_set_pitch(voice, final.pitch)
	audio.sound_set_looping(voice, final.loop == true)

	local ok, start_err = audio.sound_start(voice)
	if not ok then
		audio.sound_uninit(voice)
		return nil, start_err
	end

	local id = next_voice_id()
	voices[id] = voice
	return id
end

function S.has_bus(name)
	return groups[name] ~= nil
end

function S.voice_stop(id, fade_seconds)
	local voice = voices[id]
	if not voice then
		return false
	end
	return audio.sound_stop(voice, seconds_to_ms(fade_seconds)) ~= nil
end

function S.voice_playing(id)
	local voice = voices[id]
	if not voice then
		return false
	end
	local playing = audio.sound_playing(voice)
	if not playing then
		release_voice(id)
	end
	return playing
end

function S.voice_set_volume(id, volume)
	local voice = voices[id]
	if not voice then
		return false
	end
	audio.sound_set_volume(voice, volume)
	return true
end

function S.voice_set_pan(id, pan)
	local voice = voices[id]
	if not voice then
		return false
	end
	audio.sound_set_pan(voice, pan)
	return true
end

function S.voice_set_pitch(id, pitch)
	local voice = voices[id]
	if not voice then
		return false
	end
	audio.sound_set_pitch(voice, pitch)
	return true
end

function S.voice_set_loop(id, loop)
	local voice = voices[id]
	if not voice then
		return false
	end
	audio.sound_set_looping(voice, loop)
	return true
end

function S.voice_seek(id, seconds)
	local voice = voices[id]
	if not voice then
		return false
	end
	return audio.sound_seek(voice, seconds) ~= nil
end

function S.voice_tell(id)
	local voice = voices[id]
	if not voice then
		return nil, "Voice not found"
	end
	return audio.sound_tell(voice)
end

function S.bus_set_volume(name, volume)
	local group = groups[name]
	if group == nil then
		return false
	end
	audio.group_set_volume(group, volume)
	return true
end

function S.quit()
	is_quit = true
	for id in pairs(voices) do
		release_voice(id)
	end
	for name, group in pairs(groups) do
		audio.group_uninit(group)
		groups[name] = nil
	end
	device = nil
	definitions = {}
	bundles = {}
end

return S
