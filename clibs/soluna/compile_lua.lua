local lm = require "luamake"
local fs = require "bee.filesystem"

local function compile_lua_code(script, src, name)
  local dep = name .. "_lua_code"
  local target = lm.builddir .. "/" .. name
  local bindir = lm.bindir
  if lm.platform == "emcc" then
    bindir = lm.osbindir
  end
  lm:runlua(dep) {
    script = lm.basedir .. "/clibs/soluna/runlua.lua",
    deps = {
      "lua",
    },
    inputs = lm.basedir .. "/" .. src,
    outputs = lm.basedir .. "/" .. target,
    args = {
      bindir,
      lm.basedir .. "/" .. script,
      "$in",
      "$out",
    },
  }
  return dep
end

local lua_code_src = {
  "3rd/ltask/service",
  "3rd/ltask/lualib",
  "src/service",
  "src/lualib",
}

return function(objdeps)
  for _, dir in ipairs(lua_code_src) do
    for path in fs.pairs(lm.basedir .. "/" .. dir) do
      if path:extension() == ".lua" then
        local base = path:stem():string()
        local dep = compile_lua_code("script/lua2c.lua", path:string(), base .. ".lua.h")
        objdeps[#objdeps + 1] = dep
      end
    end
  end

  for path in fs.pairs("src/data") do
    if path:extension() == ".dl" then
      local base = path:stem():string()
      local dep = compile_lua_code("script/datalist2c.lua", path:string(), base .. ".dl.h")
      objdeps[#objdeps + 1] = dep
    end
  end
end

