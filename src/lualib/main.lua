local boot = require "ltask.bootstrap"
local embedsource = require "soluna.embedsource"

local wait_func

local init_func_temp = [=[
	local name = ...
	local embedsource = require "soluna.embedsource"
	package.path = [[${lua_path}]]
	package.cpath = [[${lua_cpath}]]
	_G.print_r = load(embedsource.runtime.print_r(), "@src/lualib/print_r.lua")()
	local embedcode = embedsource.service[name]
	if embedcode then
		return load(embedcode(),"=("..name..")")
	end
	local filename, err = package.searchpath(name, "${service_path}")
	if not filename then
		return nil, err
	end
	return loadfile(filename)
]=]

local function start(config)
	-- set callback message handler
	local root_config = {
		bootstrap = config.bootstrap,
		service_source = embedsource.runtime.service(),
		service_chunkname = "@3rd/ltask/lualib/service.lua",
		initfunc = init_func_temp:gsub("%$%{([^}]*)%}", {
			lua_path = package.path,
			lua_cpath = package.cpath,
			service_path = config.service_path or "",
		}),
	}

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
	local sender, sender_ud = bootstrap.external_sender(ctx)
	local logger, logger_ud = bootstrap.log_sender(ctx)
	_G.external_messsage(sender, sender_ud, logger, logger_ud)
	function wait_func()
		bootstrap.wait(ctx)
	end
end

function _G.cleanup()
	wait_func()
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

local appinfo = require "soluna.appinfo"

local app = {
	width = appinfo.width(),
	height = appinfo.height(),
}

start {
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
        {
            name = "start",
			args = { args, app },
        },
    },
}
