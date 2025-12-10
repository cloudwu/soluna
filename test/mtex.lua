-- To run this sample :
-- bin/soluna.exe test/mtex.game
local soluna = require "soluna"
local ltask = require "ltask"

soluna.set_window_title "multiple texture"

local bundle = {}

local function color(r,g,b)
	local name = string.format("%x%x%x", r,g,b)
	bundle[#bundle+1] = {
		name = r << 8 | g << 4 | b,
		filename = "@" .. name,
	}
	return {
		filename = "@" .. name,
		content = string.pack("BBBB", r << 4, g << 4, b << 4, 255),
		w = 1,
		h = 1,
	}
end

local function colors()
	local results = {}
	local n = 1
	for r = 0, 15 do
		for g = 0, 15 do
			for b = 0, 15 do
				results[n] = color(r,g,b)
				n = n + 1
			end
		end
	end
	return results
end

soluna.preload(colors())

local rects = soluna.load_sprites(bundle)

local args = ...
local batch = args.batch

local callback = {}

function callback.frame(count)
	batch:layer(10, 100, 100)
	for i = 0, 63 do
		for j = 0,63 do
			batch:add(rects[i * 64 + j], i, j)
		end
	end
	batch:layer()
end

return callback

