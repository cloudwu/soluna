local image = require "soluna.image"
local spritemgr = require "soluna.spritemgr"
local spritebundle = require "soluna.spritebundle"

global setmetatable, ipairs, pairs, assert

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

function S.loadbundle(filename)
	local b = bundle[filename]
	if not b then
		local desc = spritebundle.load(filecache, filename)
		b = {}
		for _, item in ipairs(desc) do
			local n = #item
			if n == 0 then
				local id = sprite_bank:add(item.cw, item.ch, item.x, item.y)
				sprite_bank:touch(id)	-- todo: don't touch here
				item.id = id
				sprite[id] = item
				b[item.name] = id
			else
				local pack = {}
				b[item.name] = pack
				for i = 1, n do
					local s = item[i]
					local id = sprite_bank:add(s.cw, s.ch, s.x, s.y)
					sprite_bank:touch(id)	-- todo:
					sprite[id] = s
					pack[i] = id
				end
			end
		end
		bundle[filename] = b
	end
	return b
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

return S