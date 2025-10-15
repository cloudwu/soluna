local subprocess = require "bee.subprocess"
local platform = require "bee.platform"
local src, target, lang = ...

local function find_executable(name)
  local handle = io.popen("where " .. name .. " 2>nul")
  if handle then
    local path = handle:read("*line")
    local success = handle:close()
    if success and path and path ~= "" then
      return path:gsub("%s+$", "")
    end
  end
  return name
end

local shdcexe = platform.os == "windows" and find_executable("sokol-shdc.exe") or "sokol-shdc"

local process = assert(subprocess.spawn {
  shdcexe,
  "--input",
  src,
  "--output",
  target,
  "--slang",
  lang,
  "--format",
  "sokol",
})

local code = process:wait()
if code ~= 0 then
  os.exit(code, true)
end
