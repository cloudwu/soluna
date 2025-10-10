local package = package
local table = table

global load, require, assert, select, error, tostring, print

local init_func_temp = [=[
	local name, service_path = ...
	local embedsource = require "soluna.embedsource"
	local file = require "soluna.file"
	package.path = [[${lua_path}]]
	package.cpath = [[${lua_cpath}]]
	local zipfile = [[${zipfile}]]
	if zipfile ~= "" then
		local zip = require "soluna.zip"
		zip.zipfile = zip.open(zipfile, "r")
	end
	_G.print_r = load(embedsource.runtime.print_r(), "@src/lualib/print_r.lua")()
	local packageloader = load(embedsource.runtime.packageloader(), "@src/lualib/packageloader.lua")
	packageloader()
	local function embedloader(name)
		local ename
		if name == "soluna" then
			ename = "soluna"
		else
			ename = name:match "^soluna%.(.*)"
		end
		if ename then
			local code = embedsource.lib[ename]
			if code then
				return function()
					local srcname = "src/lualib/"..ename..".lua"
					local f = load(code(), "@" .. srcname)
					return f(ename, srcname)
				end
			end
			return "no embed soluna." .. ename
		end
	end
	package.searchers[#package.searchers+1] = embedloader
	local embedcode = embedsource.service[name]
	if embedcode then
		return load(embedcode(),"=("..name..")")
	end
	local filename, err = file.searchpath(name, service_path or "${service_path}")
	if not filename then
		return nil, err
	end
	return load(file.load(filename), "@"..filename)
]=]

local function start(config)
	local boot = require "ltask.bootstrap"
	local mqueue = require "ltask.mqueue"
	local embedsource = require "soluna.embedsource"
	local event = require "soluna.event"
	local soluna_app = require "soluna.app"
	-- set callback message handler
	local root_config = {
		bootstrap = config.bootstrap,
		service_source = embedsource.runtime.service(),
		service_chunkname = "@3rd/ltask/lualib/service.lua",
		initfunc = init_func_temp:gsub("%$%{([^}]*)%}", {
			lua_path = package.path,
			lua_cpath = package.cpath,
			service_path = config.service_path or "",
			zipfile = config.args.zipfile or "",
		}),
	}
	
	local ev = event.create()
	local frame_barrier = event.create()
	
	table.insert(root_config.bootstrap, {
		name = "start",
		args = {
			config.args,
			{
				init = ev:ptr(),
				frame = frame_barrier:ptr()
			}
		},
	})

	boot.init_socket()
	local bootstrap = load(embedsource.runtime.bootstrap(), "@3rd/ltask/lualib/bootstrap.lua")()
	local core = config.core or {}
	core.external_queue = core.external_queue or 4096
	local ctx = bootstrap.start {
		core = core,
		root = root_config,
		root_initfunc = root_config.initfunc,
		mainthread = config.mainthread,
	}
	ev:wait()
	local sender, sender_ud = bootstrap.external_sender(ctx)
	local c_sendmessage = require "soluna.app".sendmessage
	local function send_message(...)
		c_sendmessage(sender, sender_ud, ...)
	end
	local logger, logger_ud = bootstrap.log_sender(ctx)
	local unpackevent = assert(soluna_app.unpackevent)
	local appmsg_queue = mqueue.new(128)
	local recvmsg = mqueue.recv
	
	local appmsg = {}
	
	function appmsg.set_title(text)
		soluna_app.set_window_title(text)
	end

	function appmsg.set_icon(data)
		soluna_app.set_icon(data)
	end
	
	local function do_appmsg(what, ...)
		local f = appmsg[what] or error ("Unknown app message " .. tostring(what))
		f(...)
	end
	
	local function dispatch_appmsg(v)
		while v do
			do_appmsg(boot.unpack_remove(v))
			v = recvmsg(appmsg_queue, appmsg)
		end
	end
	return {
		send_log = logger,
		send_log_ud = logger_ud,
		mqueue = appmsg_queue,
		cleanup = function()
			send_message "cleanup"
			bootstrap.wait(ctx)
			mqueue.delete(appmsg_queue)
			appmsg_queue = nil
		end,
		frame = function(count)
			local v = recvmsg(appmsg_queue)
			if v then
				dispatch_appmsg(v)
			end
			soluna_app.context_release()
			send_message("frame", count)
			frame_barrier:wait()
			soluna_app.context_acquire()
		end,
		event = function(ev)
			send_message(unpackevent(ev))
		end,
	}
end

local args = ... or {}

for i = 2, select("#", ...) do
	args[i-1] = select(i, ...)
end

if args.path then
	package.path = args.path
end

if args.cpath then
	package.cpath = args.cpath
end

local api = {}

function api.start(app)
	args.app = app
	return start {
		args = args,
		core = {
			debuglog = "=", -- stdout
		},
		bootstrap = {
			{
				name = "timer",
				unique = true,
			},
			{
				name = "log",
				unique = true,
			},
			{
				name = "loader",
				unique = true,
			},
		},
	}
end

function api.init(desc)
	-- todo : settings
	local zipfile = args[1] or args.zipfile or "main.zip"
	local zip = require "soluna.zip"
	local f = zip.open(zipfile, "r")
	if f then
		zip.zipfile = f
		args.zipfile = zipfile
		if zipfile == args[1] then
			table.remove(args, 1)
		end
	else
		args.zipfile = nil
	end
	local embedsource = require "soluna.embedsource"
	local packageloader = load(embedsource.runtime.packageloader(), "@src/lualib/packageloader.lua")
	packageloader()
	local initsetting = load(embedsource.lib.initsetting, "@3rd/ltask/lualib/initsetting.lua")()
	local settings = initsetting.init(args)
	local soluna_app = require "soluna.app"
	soluna_app.init_desc(desc, settings)
end

return api

