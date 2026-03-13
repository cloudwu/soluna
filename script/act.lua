local platform = require "bee.platform"
local fs = require "bee.filesystem"
local subprocess = require "bee.subprocess"

local is_windows = platform.os == "windows"

local function quote_ps(value)
    return "'" .. value:gsub("'", "''") .. "'"
end

local function cmdline(args)
    local out = {}
    for i = 1, #args do
        local v = args[i]
        if v:find("[%s\"]") then
            out[#out + 1] = '"' .. v:gsub('"', '\\"') .. '"'
        else
            out[#out + 1] = v
        end
    end
    return table.concat(out, " ")
end

local function run(args, option)
    option = option or {}
    print("> " .. cmdline(args))
    local process, errmsg = subprocess.spawn {
        args,
        searchPath = true,
        stdout = option.stdout ~= nil and option.stdout or io.stdout,
        stderr = option.stderr ~= nil and option.stderr or "stdout",
    }
    assert(process, errmsg)
    local code = process:wait()
    process:detach()
    if code ~= 0 then
        error(("command failed (%d): %s"):format(code, cmdline(args)))
    end
end

local function run_code(args)
    local process = assert(subprocess.spawn {
        args,
        searchPath = true,
        stdout = true,
        stderr = true,
    })
    local code = process:wait()
    process:detach()
    return code
end

local function exists(path)
    return fs.exists(path)
end

local function getenv(name, default)
    local value = os.getenv(name)
    if value == nil or value == "" then
        return default
    end
    return value
end

local function reset_dir(path)
    pcall(fs.remove_all, path)
    if not exists(path) then
        fs.create_directories(path)
    end
end

local function get_home()
    if is_windows then
        local userprofile = os.getenv("USERPROFILE")
        if userprofile and userprofile ~= "" then
            return fs.path(userprofile)
        end
        local home_drive = os.getenv("HOMEDRIVE") or ""
        local home_path = os.getenv("HOMEPATH") or ""
        local combined = home_drive .. home_path
        if combined ~= "" then
            return fs.path(combined)
        end
        return nil
    end
    local home = os.getenv("HOME")
    if home and home ~= "" then
        return fs.path(home)
    end
    return nil
end

local function detect_python()
    if is_windows then
        if run_code { "where", "py" } == 0 then
            return { "py", "-3" }
        end
        if run_code { "where", "python" } == 0 then
            return { "python" }
        end
    else
        if run_code { "which", "python3" } == 0 then
            return { "python3" }
        end
        if run_code { "which", "python" } == 0 then
            return { "python" }
        end
    end
    error "python interpreter not found"
end

local function find_file(root, filename)
    for path, status in fs.pairs_r(root) do
        if status and status:is_regular_file() and path:filename():string() == filename then
            return path
        end
    end
    return nil
end

local function collect_files(root)
    local files = {}
    if not exists(root) then
        return files
    end
    for path, status in fs.pairs_r(root) do
        if status and status:is_regular_file() then
            files[#files + 1] = path
        end
    end
    table.sort(files, function(a, b)
        return a:string() < b:string()
    end)
    return files
end

local workflow = (arg[1] or "pages"):lower()
local workflow_file = ({
    pages = ".github/workflows/pages.yml",
    nightly = ".github/workflows/nightly.yml",
})[workflow]

if not workflow_file then
    error(("unknown workflow: %s (expected: pages or nightly)"):format(workflow))
end

local function parse_options(argv)
    local options = {
        port = tonumber(getenv("PORT", "8080")) or 8080,
        host_os = platform.os,
    }
    for i = 2, #argv do
        local value = argv[i]
        local port_num = tonumber(value)
        local kv_key, kv_value = value:match("^([%w_]+)=(.+)$")
        if kv_key == "port" then
            local p = tonumber(kv_value)
            if p then
                options.port = p
            end
        elseif kv_key == "host_os" then
            options.host_os = kv_value:lower()
        elseif port_num then
            options.port = port_num
        end
    end
    return options
end

local options = parse_options(arg)
local port = options.port
local home = assert(get_home(), "home directory is not set")
local root = fs.path(getenv("ACT_ROOT", (home / ".act/soluna"):string()))
local artifact_root = root / "artifacts"
local unpack_dir = root / "unpack"
local serve_root = root / "serve"
local preview_dir = serve_root / "soluna"

reset_dir(artifact_root)
reset_dir(unpack_dir)
reset_dir(preview_dir)

local act_args = {
    "act",
    "workflow_dispatch",
    "-W",
    workflow_file,
    "--container-architecture",
    "linux/amd64",
    "--artifact-server-path",
    artifact_root:string(),
}
if workflow == "nightly" then
    local matrix_os = ({
        windows = "windows-latest",
        macos = "macos-latest",
    })[options.host_os] or "ubuntu-latest"
    act_args[#act_args + 1] = "--matrix"
    act_args[#act_args + 1] = "os:" .. matrix_os
    if matrix_os == "windows-latest" or matrix_os == "macos-latest" then
        act_args[#act_args + 1] = "-P"
        act_args[#act_args + 1] = matrix_os .. "=-self-hosted"
    end
    print(("Nightly matrix selected: %s (host_os=%s)"):format(matrix_os, options.host_os))
end
run(act_args)

if workflow ~= "pages" then
    print("Workflow completed: " .. workflow)
    print("Artifacts root: " .. artifact_root:string())
    local files = collect_files(artifact_root)
    if #files == 0 then
        print("No artifact files were found under artifacts root.")
    else
        print("Artifact files:")
        for i = 1, #files do
            local rel = files[i]:string():sub(#artifact_root:string() + 2)
            print(" - " .. rel)
        end
    end
    return
end

local zip_path = find_file(artifact_root, "github-pages.zip")
if not zip_path then
    error(table.concat({
        "missing github-pages.zip under: " .. artifact_root:string(),
        "The workflow build job was likely skipped (for example, unsupported runner image).",
        "You can inspect with: act -l ; act -n -W .github/workflows/pages.yml",
        "If needed, pass platform mapping manually, for example:",
        "  act workflow_dispatch -W .github/workflows/pages.yml -P ubuntu-latest=ghcr.io/catthehacker/ubuntu:act-latest",
    }, "\n"))
end

if is_windows then
    run {
        "powershell",
        "-NoProfile",
        "-Command",
        "Expand-Archive -LiteralPath "
            .. quote_ps(zip_path:string())
            .. " -DestinationPath "
            .. quote_ps(unpack_dir:string())
            .. " -Force",
    }
else
    run {
        "unzip",
        "-o",
        zip_path:string(),
        "-d",
        unpack_dir:string(),
    }
end

local artifact_tar = find_file(unpack_dir, "artifact.tar")
if not artifact_tar then
    error("missing artifact.tar under: " .. unpack_dir:string())
end

run {
    "tar",
    "-xf",
    artifact_tar:string(),
    "-C",
    preview_dir:string(),
}

print("Preview files are ready at: " .. preview_dir:string())
print("Serving http://127.0.0.1:" .. port .. "/soluna/")
print "If the page loads before the service worker takes control, refresh once."

local python = detect_python()
python[#python + 1] = "-m"
python[#python + 1] = "http.server"
python[#python + 1] = tostring(port)
python[#python + 1] = "--directory"
python[#python + 1] = serve_root:string()
run(python)
