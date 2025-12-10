local image = require "soluna.image"
local spritemgr = require "soluna.spritemgr"
local spritebundle = require "soluna.spritebundle"

global setmetatable, ipairs, pairs, assert, type

local sprite_bank

-- todo: make weak table
local filecache = setmetatable({ __missing = {}} , { __index = spritebundle.loadimage })

local S = {}

function S.init(config)
	sprite_bank = spritemgr.newbank(config.max_sprite, config.texture_size)
	return sprite_bank:ptr()
end

local bundle = {}
local sprite = {}

local function add_list(desc)
	local b = {}
	for _, item in ipairs(desc) do
		local n = #item
		if n == 0 then
			local id = sprite_bank:add(item.cw, item.ch, item.x, item.y)
			item.id = id
			sprite[id] = item
			b[item.name] = id
		else
			local pack = {}
			b[item.name] = pack
			for i = 1, n do
				local s = item[i]
				local id = sprite_bank:add(s.cw, s.ch, s.x, s.y)
				sprite[id] = s
				pack[i] = id
			end
		end
	end
	return b
end

local function load_from_file(filename)
	local b = bundle[filename]
	if b then
		return b
	end
	local desc = spritebundle.load(filecache, filename)
	local b = add_list(desc)
	bundle[filename] = b
	return b
end

local function load_from_table(t)
	local desc = spritebundle.load(filecache, t, t.path)
	return add_list(desc)
end

function S.loadbundle(filename)
	if type(filename) == "table" then
		return load_from_table(filename)
	else
		return load_from_file(filename)
	end
end

function S.pack()
	local texid, n = sprite_bank:pack()
	-- upload rects into n textures, [texid, texid + n)
	local results = {}
	local texid_from = texid
	for i = 1, n do
		local r = sprite_bank:altas(texid)
		for id,v in pairs(r) do
			local x = v >> 32
			local y = v & 0xffffffff
			local obj = sprite[id]
			local c = filecache[obj.filename]
			local data = image.canvas(c.data, c.w, c.h, obj.cx, obj.cy, obj.cw, obj.ch)
			local w, h, ptr = image.canvas_size(data)
			r[id] = { id = id, data = ptr, x = x, y = y, w = w, h = h, stride = c.w * 4, dx = obj.x, dy = obj.y }
		end
		texid = texid + 1
		results[i] = r
	end
	return results, texid_from
end

function S.write(id, filename)
	local obj = sprite[id]
	assert(obj.cx)
	local c = filecache[obj.filename]
	local data = image.canvas(c.data, c.w, c.h, obj.cx, obj.cy, obj.cw, obj.ch)
	local img = image.new(obj.cw, obj.ch)
	image.blit(img:canvas(), data)
	img:write(filename)
end

function S.preload(filename, content, w, h)
	assert(#content == w * h * 4)
	filecache[filename] = { data = content, w = w, h = h }
end

return S