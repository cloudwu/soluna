local subprocess = require "bee.subprocess"

local ninja_file, output = ...

assert(ninja_file, "missing ninja file path")
assert(output, "missing output path")

local process = assert(subprocess.spawn {
	"ninja",
	"-f",
	ninja_file,
	"-t",
	"compdb",
	"-x",
	searchPath = true,
	stdout = true,
})

local content = process.stdout:read "a"
local code = process:wait()
if code ~= 0 then
	os.exit(code, true)
end

local file <close> = assert(io.open(output, "wb"))
file:write(content)
