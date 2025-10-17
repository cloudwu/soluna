local lm = require "luamake"
local fs = require "bee.filesystem"

lm.rootdir = lm.basedir .. "/3rd/yoga"

lm:source_set "yoga_src" {
  sources = {
    "yoga/*.cpp",
    "yoga/*/*.cpp",
  },
  includes = {
    lm.rootdir,
  }
}

local deps = {}

local function compile_lua_code(script, src, name)
  local dep = name .. "_lua_code"
  deps[#deps + 1] = dep
  local target = lm.builddir .. "/" .. name
  local bindir = lm.bindir
  if lm.platform == "emcc" then
    bindir = lm.osbindir
  end
  lm:runlua(dep) {
    script = lm.basedir .. "/clibs/yoga/runlua.lua",
    deps = {
      "yoga_src",
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
end

local lua_code_src = {
  "3rd/ltask/service",
  "3rd/ltask/lualib",
  "src/service",
  "src/lualib",
}

for _, dir in ipairs(lua_code_src) do
  for path in fs.pairs(lm.basedir .. "/" .. dir) do
    if path:extension() == ".lua" then
      local base = path:stem():string()
      compile_lua_code("script/lua2c.lua", path:string(), base .. ".lua.h")
    end
  end
end

for path in fs.pairs("src/data") do
  if path:extension() == ".dl" then
    local base = path:stem():string()
    compile_lua_code("script/datalist2c.lua", path:string(), base .. ".dl.h")
  end
end

lm:phony "compile_lua_code" {
  deps = deps,
}
