local ltask = require "ltask"
local file = require "soluna.file"
local spritemgr = require "soluna.spritemgr"
local soluna = require "soluna"
local soluna_app = require "soluna.app"
local util = require "soluna.util"
local table = table
local debug = debug

global error, tostring, assert, load, type, ipairs, pairs, xpcall, print, pcall

local message_unpack = soluna_app.unpackmessage

local args = ...

local S = {}

local app = {}
local prehook = {}

function app.cleanup()
	ltask.send(1, "quit_ltask")
end

local init_token
function app.frame(count)
	local f
	if init_token == nil then
		init_token = {}
		f = ltask.wait(init_token)
	else
		f = init_token
	end
	init_token = nil
	f(count)
end

local function frame_callback(f)
	if init_token then
		ltask.wakeup(init_token, f)
	else
		init_token = f
	end
end

local render_service
local pre_size

function prehook.window_resize(w, h)
	if render_service then
		ltask.call(render_service, "resize", w, h)
	else
		pre_size = { width = w, height = h }
	end
end

-- external message from soluna host
function S.external(p)
	local what, arg1, arg2 = message_unpack(p)
	
	local pre = prehook[what]
	if pre then
		pre(arg1, arg2)
	end
	local f = app[what]
	if f then
		f(arg1, arg2)
	end
end

local cleanup = util.func_chain()

local function init(arg)
	if arg == nil then
		error "No command line args"
	end
	soluna.gamepad_init()
	local settings = ltask.uniqueservice "settings"
	ltask.call(settings, "init", arg)
	
	local setting = soluna.settings()
	if setting.service_path then
		ltask.servicepath(setting.service_path)
	end
	local loader = ltask.uniqueservice "loader"
	
	arg.app.bank_ptr = ltask.call(loader, "init", {
		max_sprite = setting.sprite_max,
		texture_size = setting.texture_size,
	})
	
	local entry = setting.entry
	local source = entry and file.load(entry)
	if not source then
		error ("Can't load entry " .. tostring(entry))
	end
	local f = assert(load(source, "@"..entry, "t"))

	local render = ltask.uniqueservice "render"
	
	local function init_render()
		ltask.call(render, "init", arg.app)
		render_service = render
		if pre_size then
			ltask.call(render, "resize", pre_size.width, pre_size.height)
			arg.app.width = pre_size.width
			arg.app.height = pre_size.height
			pre_size = nil
		end
		
		local batch = spritemgr.newbatch()
		cleanup:add(function()
			batch:release()
		end)
		
		local callback = f {
			batch = batch,
			width = arg.app.width,
			height = arg.app.height,
			table.unpack(arg),
		}
		
		if type(callback) ~= "table" then
			soluna_app.close_window()
			return
		end
		
		local frame_cb = callback.frame
		
		local messages = { "mouse_move", "mouse_button", "mouse_scroll", "mouse", "window_resize", "char", "key" }
		local avail = {}
		for _, v in ipairs(messages) do
			avail[v] = true
		end
		for k,v in pairs(callback) do
			if avail[k] then
				app[k] = v
			end
		end
		
		local batch_id = ltask.call(render, "register_batch", ltask.self())

		local function frame(count)
			batch:reset()
			frame_cb(count)
			ltask.send(render, "submit_batch", batch_id, batch:ptr())
			ltask.call(render, "frame", count)
		end
		
		local traceback = debug.traceback
		
		local function render_frame(count)
			local ok, err = xpcall(frame, traceback, count)
			if not ok then
				function app.frame()
					ltask.mainthread_run(function() end)
				end
				error(err)
			end
		end
		
		frame_callback(function()
			-- replace app.frame
			app.frame = render_frame
			render_frame()
		end)
	end
	
	frame_callback(function ()
		-- init render in the first frame, because render init would call some gfx api
		local ok, err = xpcall(init_render, debug.traceback)
		if not ok then
			print(err)
			soluna_app.close_window()
		end
	end)
end

function S.quit()
	cleanup()
end

ltask.fork(function()
	ltask.call(1, "external_forward", ltask.self(), "external")

	-- trigger INIT_EVENT, see main.lua
	ltask.mainthread_run(function() end)
	
	local ok , err = pcall(init, args)
	if not ok then
		ltask.log.error(err)
		soluna_app.quit()
	end
end)


return S
