local lm = require "luamake"
local subprocess = require "bee.subprocess"

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

lm:source_set "soluna_src" {
  sources = {
    "src/*.c",
  },
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
  macos = {
    frameworks = {
      "IOKit",
      "CoreText",
      "CoreFoundation",
      "Foundation",
      "Cocoa",
      "Metal",
      "MetalKit",
      "QuartzCore",
    },
    flags = {
      "-x objective-c",
    },
  },
  windows = {
    includes = {
      "3rd/zlib/contrib/minizip",
    }
  },
  linux = {
    links = {
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
