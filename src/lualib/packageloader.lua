local file = require "soluna.file"
local zip = require "soluna.zip"
local lfs = require "soluna.lfs"

local package = package
local string = string
local io = io

global load, print, setmetatable, table, type, tostring, ipairs, require, error

local dir_sep, temp_sep, temp_marker = package.config:match "(.)\n(.)\n(.)"
local temp_pat = "[^"..temp_sep.."]+"

local function load_zips(zipnames)
	if zipnames == nil then
		return
	end
	local n = 0
	local r = {}
	for fullname in zipnames:gmatch "[^:;]+" do
		local name, root = fullname:match "(.-)@(.*)"
		if name then
			root = root .. "/"
		else
			name = fullname
		end
		local zf = zip.open(name, "r")
		if not zf then
--			print("Can't open patch", name)
		else
--			print("Load patch", name)
			n = n + 1
			r[n] = { zip = zf, root = root }
		end
	end
	r.n = n
	if n > 0 then
		return r
	else
--		print("No zip, use local files")
	end
end

local zipfile = load_zips(...)
local file_load = file.load
local file_exist = file.exist

if zipfile then
	local function find_file(cache, fullname)
		local name = fullname:match "%./(.*)" or fullname
		for i = zipfile.n, 1, -1 do
			local root = zipfile[i].root
			local name_in_zip
			if root then
				local n = #root
				if name:sub(1, n) == root then
					name_in_zip = name:sub(n+1)
				end
			else
				name_in_zip = name
			end
			local zf = zipfile[i].zip
			if name_in_zip and zf:exist(name_in_zip) then
				cache[name] = function()
					return zf:readfile(name_in_zip)
				end
--				print(name, "in zipfile", i)
				return cache[name]
			end
		end
	end
	local list
	local names_cache = setmetatable({}, { __index = find_file})

	function file_load(name)
		local loader = names_cache[name] or error ("Can't load ".. name)
		return loader()
	end
	function file_exist(name)
		return names_cache[name] ~= nil
	end
	file.local_load = file.load
	file.local_exist = file.exist
	file.load = file_load
	file.exist = file_exist
	local function gen_list()
		local tmp = {}
		local r = {}
		local n = 1
		for i = zipfile.n, 1, -1 do
			local flist = zipfile[i].zip:list()
			local root = zipfile[i].root
			for j = 1, #flist do
				local name = flist[j]
				if root then
					name = root and root .. name
				end
				if tmp[name] == nil then
					tmp[name] = true
					-- todo : add path of name
				end
				r[n] = name
				n = n + 1
			end
		end
		table.sort(r)
		return r
	end
	function file.dir(root)
		list = list or gen_list()
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
		list = list or gen_list()
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

return zipfile