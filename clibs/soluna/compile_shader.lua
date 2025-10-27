local lm = require "luamake"
local fs = require "bee.filesystem"

local shdc<const> = lm.shdc or ""

local function compile_shader(src, name, lang)
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
      lang,
    },
  }
  return dep
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

return function(objdeps)
  for path in fs.pairs("src") do
    local lang = shader_lang()
    if path:extension() == ".glsl" then
      local base = path:stem():string()
      local dep = compile_shader(path:string(), base .. ".glsl.h", lang)
      objdeps[#objdeps + 1] = dep
    end
  end
end
