local ltask = require "ltask"
local spritemgr = require "soluna.spritemgr"
local mattext = require "soluna.material.text"
local font = require "soluna.font"
local soluna = require "soluna"

local arg, app = ...

local external = ltask.spawn "external"

ltask.call(external, "init", app)

local function font_init()
	local sysfont = require "soluna.font.system"
	font.import(assert(sysfont.ttfdata "微软雅黑"))
	return font.name ""
end

local sprites
local loader = ltask.uniqueservice "loader"
local render = ltask.uniqueservice "render"
local sprites = ltask.call(loader, "loadbundle", "asset/sprites.dl")
local batch = spritemgr.newbatch()

local batch_id = ltask.call(render, "register_batch", ltask.self())
local font_id = font_init()
local quit = false

local function text(c, color)
	local cp = utf8.codepoint(c)
	return mattext.char(cp, font_id, 24, color)
end

local function mainloop()
	local count = 0
	soluna.gamepad_init()
	while not quit do
		if soluna.gamepad.A then
			count = count + 1
		end
		local rad = count * 3.1415927 / 180
		local scale = math.sin(rad)
		batch:reset()
		batch:add(sprites.avatar, 256, 200, scale + 1.2, rad)
		batch:add(text ("你", 0xff0000), 10, 30)
		batch:add(sprites.avatar, 256, 400, scale + 1.2, -rad)
		batch:add(text ("好", 0x0000ff), 40, 60)
		batch:add(sprites.avatar, 256, 600, - scale + 1.2, rad)
		ltask.call(render, "submit_batch", batch_id, batch:ptr())
	end
	if quit ~= "finish" then
		ltask.wakeup(quit)
	end
	quit = "finish"
end

local token = ltask.fork(mainloop)

local S = {}

function S.quit()
	if not quit then
		quit = true
	elseif quit ~= "finish" then
		quit = {}
		ltask.wait(quit)
	end
end

-- todo:

local filename = arg[1]

if filename then
	dofile(filename)
end

return S
