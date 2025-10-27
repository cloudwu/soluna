local lm = require "luamake"
local subprocess = require "bee.subprocess"
local compile_lua = require "compile_lua"
local compile_shader = require "compile_shader"

lm.rootdir = lm.basedir

local ok, process, errMsg = pcall(subprocess.spawn, {
  lm.os ~= "windows" and "git" or "C:\\Program Files\\Git\\cmd\\git.exe",
  "rev-parse",
  "HEAD",
  stdout = true,
})
local commit
if ok then
  if errMsg then
    print("Failed to start git process: " .. errMsg)
  else
    local output = process.stdout:read "a"
    commit = output:match "^%s*(.-)%s*$"
    process:wait()
    print("Hash version: " .. commit)
  end
end

local objdeps = {}

compile_lua(objdeps)
compile_shader(objdeps)

lm:source_set "soluna_src" {
  sources = {
    "src/*.c",
  },
  objdeps = objdeps,
  defines = {
    commit and string.format('SOLUNA_HASH_VERSION=\\"%s\\"', commit),
  },
  includes = {
    "build",
    "3rd/lua",
    "3rd",
    "3rd/yoga",
    "3rd/zlib",
  },
  clang = {
    sources = lm.os == "macos" and {
      "src/platform/macos/*.m",
    },
    flags = lm.os == "macos" and {
      "-x objective-c",
    },
    frameworks = lm.os == "macos" and {
      "IOKit",
      "CoreText",
      "CoreFoundation",
      "Foundation",
      "Cocoa",
      "Metal",
      "MetalKit",
      "QuartzCore",
    },
  },
  windows = {
    sources = {
      "src/platform/windows/*.c",
    },
    includes = {
      "3rd/zlib/contrib/minizip",
    }
  },
  gcc = {
    sources = lm.os == "linux" and {
      "src/platform/linux/*.c",
    } or nil,
    links = lm.os == "linux" and {
      "pthread",
      "dl",
      "GL",
      "X11",
      "Xrandr",
      "Xi",
      "Xxf86vm",
      "Xcursor",
      "GLU",
      "asound",
    },
  },
  msvc = {
    ldflags = {
      "-SUBSYSTEM:WINDOWS",
      "xinput.lib",
      "Ws2_32.lib",
      "ntdll.lib",
      "Imm32.lib",
    },
  },
  mingw = {
    links = {
      "kernel32",
      "user32",
      "shell32",
      "gdi32",
      "dxgi",
      "d3d11",
      "winmm",
      "ws2_32",
      "ntdll",
      "xinput",
      "imm32",
    },
    flags = {
      "-Wl,subsystem,windows",
    },
  },
}
