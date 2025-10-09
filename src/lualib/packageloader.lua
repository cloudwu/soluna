local file = require "soluna.file"

local package = package
local string = string
local io = io

global load, print

local dir_sep, temp_sep, temp_marker = package.config:match "(.)\n(.)\n(.)"
local temp_pat = "[^"..temp_sep.."]+"

local function fileload(name, fullname)
	local s, err = file.load(fullname)
	local f = load(s, "@"..fullname)
	return f(name, fullname)
end

local function search_file(name)
	local cname = name:gsub("%.", "/")
	for temp in package.path:gmatch(temp_pat) do
		local fullname = temp:gsub(temp_marker, cname)
		if dir_sep ~= '/' then
			fullname = fullname:gsub(dir_sep, "/")
		end
		if file.exist(fullname) then
			return fileload, fullname
		end
	end
	return "No package : " .. name
end

package.searchers[2] = search_file
