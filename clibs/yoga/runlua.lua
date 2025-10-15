local subprocess = require "bee.subprocess"
local platform = require "bee.platform"

local bindir, script, src, target = ...

local luaexe = platform.os == "windows" and bindir .. "/lua.exe" or bindir .. "/lua"

local process = assert(subprocess.spawn {
  luaexe, script, src, target,
})

local code = process:wait()
if code ~= 0 then
  os.exit(code, true)
end
