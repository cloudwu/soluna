local lm = require "luamake"
local fs = require "bee.filesystem"

local function detect_emcc()
	if lm.compiler == "emcc" then
		return true
	end
	if type(lm.cc) == "string" and lm.cc:find("emcc", 1, true) then
		return true
	end
	return false
end

local osplat = (function()
	if lm.os == "windows" then
		if lm.compiler == "gcc" then
			return "mingw"
		end
		if lm.cc == "clang-cl" then
			return "clang-cl"
		end
		return "msvc"
	end
	return lm.os
end)()

local plat = (function()
	if detect_emcc() then
		return "emcc"
	end
	return osplat
end)()

lm.platform = plat
lm.basedir = lm:path "."
lm.bindir = ("bin/%s/%s"):format(plat, lm.mode)
lm.osbindir = ("bin/%s/%s"):format(osplat, lm.mode)

lm:conf {
	cxx = "c++20",
	clang = {
		c = "c11",
	},
	flags = {
		lm.mode ~= "debug" and "-O2",
	},
	msvc = {
		c = "c11",
		flags = {
			"-W3",
			"-utf-8",
			"-experimental:c11atomics",
			"/wd4244",
			"/wd4267",
			"/wd4305",
			"/wd4996",
			"/wd4018",
			"/wd4113",
		},
		defines = {
			"_CRT_SECURE_NO_WARNINGS",
			"_CRT_NONSTDC_NO_DEPRECATE",
			"_CRT_SECURE_NO_DEPRECATE"
		},
	},
	mingw = {
		c = "c99",
	},
	gcc = {
		c = "c11",
		flags = {
			"-Wall",
		},
		defines = {
			"_POSIX_C_SOURCE=199309L",
			"_GNU_SOURCE",
		},
		links = {
			"m",
			(lm.os ~= "windows" and lm.platform ~= "emcc") and "fontconfig",
		},
	},
	emcc = {
		c = "gnu11",
		flags = {
			"-Wall",
			"-pthread",
			"-fPIC",
			"--use-port=emdawnwebgpu",
			"-fwasm-exceptions",
		},
		links = {
			"idbfs.js",
		},
		ldflags = {
			"--use-port=emdawnwebgpu",
			"-s ALLOW_MEMORY_GROWTH",
			"-s FORCE_FILESYSTEM=1",
			"-s USE_PTHREADS=1",
			"-fwasm-exceptions",

			lm.mode == "debug" and "-gsource-map",
			lm.mode == "debug" and "-s EXCEPTION_STACK_TRACES=1",
			lm.mode == "debug" and "-s ASSERTIONS=2",
			-- lm.mode == "debug" and "-s SAFE_HEAP=1",
			lm.mode == "debug" and "-s STACK_OVERFLOW_CHECK=1",
			lm.mode == "debug" and "-s PTHREADS_DEBUG=1",
		},
		defines = {
			"_POSIX_C_SOURCE=200809L",
			"_GNU_SOURCE",
		},
	},
	defines = {
		-- lm.mode == "debug" and "DEBUGLOG",
		lm.mode == "debug" and "SOKOL_DEBUG",
	}
}

local deps = { "soluna_src" }

for path in fs.pairs(lm.basedir .. "/clibs") do
	local name = path:stem():string()
	if name ~= "soluna" and name ~= "sample" and fs.exists(path / "make.lua") then
		local makefile = ("clibs/%s/make.lua"):format(name)
		lm:import(makefile)
		deps[#deps + 1] = name .. "_src"
	end
end

lm:import "clibs/soluna/make.lua"
lm:import "clibs/sample/make.lua"

lm:exe "soluna" {
	deps = deps,
	emcc = {
		ldflags = {
			"--js-library=src/platform/wasm/soluna_ime.js",
			"--js-library=src/platform/wasm/soluna_openurl.js",
			"-s MODULARIZE=1",
			"-s EXPORT_ES6=1",
			"-s EXPORT_NAME=createApp",
			'-s EXPORTED_FUNCTIONS=\'["_main"]\'',
			'-s EXPORTED_RUNTIME_METHODS=\'["FS","FS_createPath","FS_createDataFile","IDBFS"]\'',
			"-s PTHREAD_POOL_SIZE='Math.max(2,navigator.hardwareConcurrency)'",
			"-s PTHREAD_POOL_SIZE_STRICT=2",
			"-s MAIN_MODULE=2",
			"-Wl,-u,emscripten_builtin_memalign",
			"-Wl,--export=emscripten_builtin_memalign",
		},
	},
}

lm:import "script/act_targets.lua"

lm:runlua "cc" {
	script = "script/compile_commands.lua",
	args = {
		"build/build.ninja",
		"compile_commands.json",
	},
	inputs = {
		"build/build.ninja",
		"script/compile_commands.lua",
	},
	outputs = {
		"compile_commands.json",
	},
}

lm:default "soluna"
