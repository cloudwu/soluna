local function trim(s)
  return (s:gsub("^%s+", ""):gsub("%s+$", ""))
end

local function shell_quote(value)
  return "'" .. value:gsub("'", "'\\''") .. "'"
end

local function run(cmd)
  local ok = os.execute(cmd)
  if ok ~= true and ok ~= 0 then
    error("command failed: " .. cmd)
  end
end

local function read_file(path)
  local f = assert(io.open(path, "rb"))
  local data = f:read("*a")
  f:close()
  return data
end

local function write_file(path, data)
  local f = assert(io.open(path, "wb"))
  f:write(data)
  f:close()
end

local function write_lines(path, lines)
  write_file(path, table.concat(lines, "\n") .. "\n")
end

local function read_lines(path)
  local f = assert(io.open(path, "r"))
  local lines = {}
  for line in f:lines() do
    lines[#lines + 1] = line
  end
  f:close()
  return lines
end

local function exec_lines(cmd)
  local p = assert(io.popen(cmd, "r"))
  local lines = {}
  for line in p:lines() do
    if line ~= "" then
      lines[#lines + 1] = line
    end
  end
  p:close()
  return lines
end

local function is_array(tbl)
  local count = 0
  for k, _ in pairs(tbl) do
    if type(k) ~= "number" then
      return false
    end
    count = count + 1
  end
  for i = 1, count do
    if tbl[i] == nil then
      return false
    end
  end
  return true
end

local function json_escape(value)
  return value
    :gsub("\\", "\\\\")
    :gsub("\"", "\\\"")
    :gsub("\b", "\\b")
    :gsub("\f", "\\f")
    :gsub("\n", "\\n")
    :gsub("\r", "\\r")
    :gsub("\t", "\\t")
end

local function json_encode(value)
  local t = type(value)
  if t == "nil" then
    return "null"
  elseif t == "boolean" then
    return value and "true" or "false"
  elseif t == "number" then
    return tostring(value)
  elseif t == "string" then
    return "\"" .. json_escape(value) .. "\""
  elseif t == "table" then
    if is_array(value) then
      local parts = {}
      for i = 1, #value do
        parts[#parts + 1] = json_encode(value[i])
      end
      return "[" .. table.concat(parts, ",") .. "]"
    end
    local parts = {}
    for k, v in pairs(value) do
      parts[#parts + 1] = json_encode(k) .. ":" .. json_encode(v)
    end
    table.sort(parts)
    return "{" .. table.concat(parts, ",") .. "}"
  else
    error("unsupported json type: " .. t)
  end
end

local function yaml_quote(value)
  return value:gsub("\\", "\\\\"):gsub("\"", "\\\"")
end

local function shortcode_quote(value)
  return value:gsub("\\", "\\\\"):gsub("\"", "\\\"")
end

local function html_escape(value)
  return value
    :gsub("&", "&amp;")
    :gsub("<", "&lt;")
    :gsub(">", "&gt;")
end

local function write_front_matter(lines, fields)
  lines[#lines + 1] = "---"
  for _, field in ipairs(fields) do
    lines[#lines + 1] = string.format("%s: \"%s\"", field.key, yaml_quote(field.value))
  end
  lines[#lines + 1] = "---"
end

local function titleize(name)
  local parts = {}
  for part in name:gmatch("[^_%-%s]+") do
    parts[#parts + 1] = part:sub(1, 1):upper() .. part:sub(2)
  end
  return table.concat(parts, " ")
end

local function parse_doc_file(path)
  local blocks = {}
  local doc_lines = {}
  local annos = {}

  local function flush(signature)
    if #doc_lines == 0 and #annos == 0 then
      return
    end
    blocks[#blocks + 1] = {
      signature = signature,
      docs = doc_lines,
      annos = annos,
    }
    doc_lines = {}
    annos = {}
  end

  for _, line in ipairs(read_lines(path)) do
    if line:match("^%-%-%-@") then
      annos[#annos + 1] = trim(line:gsub("^%-%-%-@", ""))
    elseif line:match("^%-%-%-") then
      doc_lines[#doc_lines + 1] = trim(line:gsub("^%-%-%-%s?", ""))
    else
      local trimmed = trim(line)
      if trimmed ~= "" and (#doc_lines > 0 or #annos > 0) then
        flush(trimmed)
      end
    end
  end
  flush(nil)
  return blocks
end

local function parse_args(argv)
  local opts = {
    soluna = ".",
    site = "web",
    wasm = nil,
    js = nil,
  }
  local i = 1
  while i <= #argv do
    local arg = argv[i]
    if arg == "--soluna" then
      opts.soluna = argv[i + 1]
      i = i + 2
    elseif arg == "--site" then
      opts.site = argv[i + 1]
      i = i + 2
    elseif arg == "--wasm" then
      opts.wasm = argv[i + 1]
      i = i + 2
    elseif arg == "--js" then
      opts.js = argv[i + 1]
      i = i + 2
    else
      error("unknown argument: " .. arg)
    end
  end
  if not opts.wasm or not opts.js then
    error("missing --wasm or --js argument")
  end
  return opts
end

local function write_examples_content(site_dir, examples)
  local examples_dir = site_dir .. "/content/examples"
  run("mkdir -p " .. shell_quote(examples_dir))

  for _, example in ipairs(examples) do
    local lines = {
      "---",
      string.format("title: \"%s\"", yaml_quote(example.title)),
      string.format("description: \"%s\"", yaml_quote("Soluna example: " .. example.entry)),
      string.format("example_id: \"%s\"", yaml_quote(example.id)),
      string.format("entry: \"%s\"", yaml_quote(example.entry)),
      "---",
    }
    write_lines(examples_dir .. "/" .. example.id .. ".md", lines)
  end
end

local function write_docs_content(site_dir, docs)
  local docs_dir = site_dir .. "/content/docs"
  run("mkdir -p " .. shell_quote(docs_dir))

  local lines = {}
  write_front_matter(lines, {
    { key = "title", value = "Docs" },
    { key = "description", value = "Soluna API reference." },
  })
  lines[#lines + 1] = ""
  lines[#lines + 1] = "{{< menubar >}}[home]({{< relurl \"/\" >}}) · [contents](#contents) · [index](#index){{< /menubar >}}"
  lines[#lines + 1] = ""
  lines[#lines + 1] = "{{< heading level=\"1\" id=\"contents\" text=\"Contents\" >}}"
  lines[#lines + 1] = ""
  for _, module in ipairs(docs) do
    lines[#lines + 1] = string.format("- [%s](#%s)", module.title, module.module)
  end
  lines[#lines + 1] = ""
  lines[#lines + 1] = "{{< heading level=\"1\" id=\"index\" text=\"Index\" >}}"
  lines[#lines + 1] = ""
  for _, module in ipairs(docs) do
    for i, block in ipairs(module.blocks) do
      local signature = block.signature or "@block"
      lines[#lines + 1] = string.format("- [%s](#%s-%d)", signature, module.module, i)
    end
  end
  lines[#lines + 1] = ""
  for _, module in ipairs(docs) do
    lines[#lines + 1] = string.format("{{< heading level=\"1\" id=\"%s\" text=\"%s\" >}}", shortcode_quote(module.module), shortcode_quote(module.title))
    if module.module ~= "" and module.title ~= "" and module.module:lower() ~= module.title:lower() then
      lines[#lines + 1] = ""
      lines[#lines + 1] = string.format("{{< small >}}%s{{< /small >}}", module.module)
    end
    lines[#lines + 1] = ""
    for i, block in ipairs(module.blocks) do
      local signature = block.signature or "@block"
      lines[#lines + 1] = string.format("{{< heading level=\"3\" id=\"%s-%d\" text=\"%s\" code=\"true\" >}}", shortcode_quote(module.module), i, shortcode_quote(signature))
      lines[#lines + 1] = ""
      if block.docs and #block.docs > 0 then
        local paragraph = {}
        local has_list = false
        local function flush_paragraph()
          if #paragraph > 0 then
            lines[#lines + 1] = table.concat(paragraph, " ")
            lines[#lines + 1] = ""
            paragraph = {}
          end
        end
        for _, doc_line in ipairs(block.docs) do
          if doc_line:match("^%- ") then
            flush_paragraph()
            lines[#lines + 1] = doc_line
            has_list = true
          else
            if has_list then
              lines[#lines + 1] = ""
              has_list = false
            end
            paragraph[#paragraph + 1] = doc_line
          end
        end
        flush_paragraph()
        if has_list then
          lines[#lines + 1] = ""
        end
      end
      if block.annos and #block.annos > 0 then
        lines[#lines + 1] = "{{< pre >}}"
        for _, anno in ipairs(block.annos) do
          lines[#lines + 1] = "@" .. anno
        end
        lines[#lines + 1] = "{{< /pre >}}"
        lines[#lines + 1] = ""
      end
    end
  end
  write_lines(docs_dir .. "/_index.md", lines)
end

local opts = parse_args(arg)
local soluna_dir = opts.soluna
local site_dir = opts.site
local runtime_dir = site_dir .. "/static/runtime"
local data_dir = site_dir .. "/data"
local static_data_dir = site_dir .. "/static/data"

run("mkdir -p " .. shell_quote(runtime_dir))
run("mkdir -p " .. shell_quote(runtime_dir .. "/test"))
run("mkdir -p " .. shell_quote(data_dir))
run("mkdir -p " .. shell_quote(static_data_dir))

run("cp " .. shell_quote(opts.wasm) .. " " .. shell_quote(runtime_dir .. "/soluna.wasm"))
run("cp " .. shell_quote(opts.js) .. " " .. shell_quote(runtime_dir .. "/soluna.js"))

run("cd " .. shell_quote(soluna_dir) .. " && zip -r " .. shell_quote(runtime_dir .. "/asset.zip") .. " asset")
run("cp -R " .. shell_quote(soluna_dir .. "/test/.") .. " " .. shell_quote(runtime_dir .. "/test"))

local examples = {}
local example_paths = exec_lines("find " .. shell_quote(soluna_dir .. "/test") .. " -maxdepth 1 -type f -name '*.lua' -print")
table.sort(example_paths)
for _, path in ipairs(example_paths) do
  local name = path:match("([^/]+)%.lua$")
  if name then
    local content = read_file(path)
    if not content:find("font%.system") then
      examples[#examples + 1] = {
        id = name,
        title = titleize(name),
        entry = "test/" .. name .. ".lua",
      }
    end
  end
end

local docs = {}
local doc_paths = exec_lines("find " .. shell_quote(soluna_dir .. "/docs") .. " -maxdepth 1 -type f -name '*.lua' -print")
table.sort(doc_paths)
for _, path in ipairs(doc_paths) do
  local name = path:match("([^/]+)%.lua$")
  if name then
    docs[#docs + 1] = {
      module = name,
      title = titleize(name),
      blocks = parse_doc_file(path),
    }
  end
end

write_examples_content(site_dir, examples)
write_docs_content(site_dir, docs)

local examples_payload = json_encode({
  generated_at = os.date("!%Y-%m-%dT%H:%M:%SZ"),
  examples = examples,
})
write_file(data_dir .. "/examples.json", examples_payload)
write_file(static_data_dir .. "/examples.json", examples_payload)

local docs_payload = json_encode({
  generated_at = os.date("!%Y-%m-%dT%H:%M:%SZ"),
  modules = docs,
})
write_file(data_dir .. "/docs.json", docs_payload)
write_file(static_data_dir .. "/docs.json", docs_payload)
