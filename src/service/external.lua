local ltask = require "ltask"
local app = require "soluna.app"

local S = {}

local render = ltask.uniqueservice "render"
local gamepad = ltask.uniqueservice "gamepad"

ltask.send(1, "external_forward", ltask.self(), "external")

local command = {}

function command.frame(_, _, count)
	ltask.send(gamepad, "update")
	ltask.call(render, "frame", count)
	app.nextframe()
end

function command.cleanup()
	app.frameready(false)
	app.nextframe()
	ltask.call(render, "quit")
	ltask.send(1, "quit_ltask")
end

local function dispatch(type, ...)
	local f =command[type]
	if f then
		f(...)
	else
--		todo:	
--		print(type, ...)
	end
end

function S.external(p)
	dispatch(app.unpackmessage(p))
end

function S.init(arg)
	ltask.call(render, "init", arg)
	app.frameready(true)
end

return S
