local lm = require "luamake"
local fs = require "bee.filesystem"

local deps = {}

local function compile_shader(src, name, lang)
  local dep = name .. "_shader"
  deps[#deps + 1] = dep
  local target = lm.builddir .. "/" .. name
  lm:runlua(dep) {
    script = lm.basedir .. "/clibs/sokol/shader2c.lua",
    inputs = lm.basedir .. "/" .. src,
    outputs = lm.basedir .. "/" .. target,
    args = {
      "$in",
      "$out",
      lang,
    },
  }
end

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

for path in fs.pairs("src") do
  local lang = shader_lang()
  if path:extension() == ".glsl" then
    local base = path:stem():string()
    compile_shader(path:string(), base .. ".glsl.h", lang)
  end
end

lm:phony "compile_shaders" {
  deps = deps,
}
