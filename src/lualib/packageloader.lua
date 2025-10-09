local file = require "soluna.file"
local zip = require "soluna.zip"
local lfs = require "soluna.lfs"

local package = package
local string = string
local io = io

global load, print

local dir_sep, temp_sep, temp_marker = package.config:match "(.)\n(.)\n(.)"
local temp_pat = "[^"..temp_sep.."]+"

local zipfile = zip.zipfile
local file_load = file.load
local file_exist = file.exist

if zipfile then
	function file_load(fullname)
		local name = fullname:match "%./(.*)" or fullname
		return zipfile:readfile(name)
	end
	function file_exist(fullname)
		local name = fullname:match "%./(.*)" or fullname
		return zipfile:exist(name)
	end
	file.local_load = file.load
	file.local_exist = file.exist
	file.load = file_load
	file.exist = file_exist
	local list
	function file.dir(root)
		list = list or zipfile:list()
		root = root:gsub("[^/]$", "%0/")
		local iter = 1
		local n = #list
		local root_n = #root
		local last
		return function()
			while iter <= n do
				local t = list[iter]
				iter = iter + 1
				if t:sub(1, root_n) == root then
					local sname = t:sub(root_n+1):match "[^/]+"
					if sname ~= last then
						last = sname
						return sname
					end
				end
			end
		end
	end
	function file.attributes(fullname)
		list = list or zipfile:list()
		local pathname = fullname .. "/"
		local pathn = #pathname
		for i = 1, #list do
			local t = list[i]
			if fullname == t then
				return "file"
			elseif t:sub(1, pathn) == pathname then
				return "directory"
			end
		end
	end
	function file.searchpath(name, path)
		local cname = name:gsub("%.", "/")
		for temp in path:gmatch(temp_pat) do
			local fullname = temp:gsub(temp_marker, cname)
			if dir_sep ~= '/' then
				fullname = fullname:gsub(dir_sep, "/")
			end
			if file_exist(fullname) then
				return fullname
			end
		end
	end
else
	file.dir = lfs.dir
	file.attributes = lfs.attributes
	file.local_load = file.load
	file.local_exist = file.exist
	file.searchpath = package.searchpath
end

local function fileload(name, fullname)
	local s, err = file_load(fullname)
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
		if file_exist(fullname) then
			return fileload, fullname
		end
	end
	return "No package : " .. name
end

package.searchers[2] = search_file
