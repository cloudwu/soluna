local subprocess = require "bee.subprocess"
local shdcexe, src, target, lang = ...

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
