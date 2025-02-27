local ltask = require "ltask"
local spritemgr = require "soluna.spritemgr"

local arg, app = ...

local external = ltask.spawn "external"

ltask.call(external, "init", app)

local filename = arg[1]

if filename then
	dofile(filename)
end

local sprites
local loader = ltask.uniqueservice "loader"
local render = ltask.uniqueservice "render"
local sprites = ltask.call(loader, "loadbundle", "asset/sprites.dl")
local batch = spritemgr.newbatch()

local batch_id = ltask.call(render, "register_batch", ltask.self())
local quit

local function mainloop()
	local count = 0
	while not quit do
		count = count + 1
		local rad = count * 3.1415927 / 180
		local scale = math.sin(rad) + 1.2
		batch:reset()
		batch:add(sprites.avatar, 256, 256, scale, rad)
		ltask.call(render, "submit_batch", batch_id, batch:ptr())
	end
end

ltask.fork(mainloop)

local S = {}

function S.quit()
	quit = true
end

return S
