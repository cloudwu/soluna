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

lm:conf({
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
      "--use-port=emdawnwebgpu",
    },
    links = {
      "idbfs.js",
    },
    ldflags = {
      '--js-library=src/wasm/soluna_ime.js',
      "--use-port=emdawnwebgpu",
      "-s FULL_ES3=1",
      "-s ALLOW_MEMORY_GROWTH",
      "-s FORCE_FILESYSTEM=1",
      '-s EXPORTED_RUNTIME_METHODS=\'["FS","FS_createPath","FS_createDataFile","IDBFS"]\'',
      "-s USE_PTHREADS=1",
      "-s PTHREAD_POOL_SIZE=30",
      "-s PTHREAD_POOL_SIZE_STRICT=2",
      "-s OFFSCREENCANVAS_SUPPORT=1",
      "-s OFFSCREEN_FRAMEBUFFER=1",
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
})

lm:import "clibs/lua/make.lua"
lm:import "clibs/yoga/make.lua"
lm:import "clibs/datalist/make.lua"
lm:import "clibs/sokol/make.lua"
lm:import "clibs/ltask/make.lua"
lm:import "clibs/zlib/make.lua"
lm:import "clibs/soluna/make.lua"

lm:phony "precompile" {
  deps = {
    "compile_shaders",
    "compile_lua_code",
  },
}

lm:exe "soluna" {
  deps = {
    "lua_src",
    "soluna_src",
    "ltask_src",
    "yoga_src",
    "precompile",
    "datalist_src",
    "zlib",
    "minizip",
  },
}
