local lm = require "luamake"
local platform = require "bee.platform"

lm.rootdir = lm.basedir

local function shdc_plat()
	if lm.os == "windows" then
		return "win32"
	end
	if lm.os == "linux" then
		return "linux"
	end
	if lm.os == "macos" then
		return platform.Arch == "arm64" and "osx_arm64" or "osx"
	end
	return "unknown"
end

local paths = {
	windows = "$PATH/$NAME.exe",
	macos = "$PATH/$NAME",
	linux = "$PATH/$NAME",
}

local shdc = assert(paths[lm.os]):gsub("%$(%u+)", {
	PATH = tostring(lm.basedir / "bin/sokol-tools-bin/bin" / shdc_plat()),
	NAME = "sokol-shdc",
})

local function shader_lang()
	local plat = lm.platform
	if plat == "msvc" or plat == "clang-cl" or plat == "mingw" then
		return "hlsl4"
	end
	if plat == "macos" then
		return "metal_macos"
	end
	if plat == "emcc" then
		return "wgsl"
	end
	if plat == "linux" then
		return "glsl430"
	end
	return "unknown"
end

local function compile_shader(src, name)
	local dep = name .. "_shader"
	local target = lm.builddir .. "/" .. name
	lm:runlua(dep) {
		script = lm.basedir .. "/clibs/soluna/shader2c.lua",
		inputs = lm.basedir .. "/" .. src,
		outputs = lm.basedir .. "/" .. target,
		args = {
			shdc,
			"$in",
			"$out",
			shader_lang(),
		},
	}
	return dep
end

local sample_shader = compile_shader("extlua/perspective_quad.glsl", "perspective_quad.glsl.h")

lm:dll "sample" {
	sources = {
		"extlua/extlua.c",
		"extlua/sokolapi.c",
		"extlua/solunaapi.c",
		"extlua/extlua_sample.c",
	},
	objdeps = {
		sample_shader,
	},
	includes = {
		"3rd/lua",
		"3rd",
		"build",
		"extlua",
	},
}
