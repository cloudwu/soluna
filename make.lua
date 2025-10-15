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
    "-O2",
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
    c = "c11",
    flags = {
      "-Wall",
    },
    ldflags = {
      "-s MIN_WEBGL_VERSION=2",
      "-s MAX_WEBGL_VERSION=2",
      "-s FULL_ES3=1",
    },
    defines = {
      "_POSIX_C_SOURCE=200809L",
      "_GNU_SOURCE",
    },
  },
  defines = {
    -- "SOKOL_DEBUG",
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
