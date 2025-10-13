local datalist = require "soluna.datalist"
local source = require "soluna.embedsource"
local lfs = require "soluna.lfs"
local file = require "soluna.file"

global type, error, pairs, assert, tonumber, print

local S = {}

local function patch(s, k, v)
	if type(k) == "number" then
		-- ignore
		return
	end
	local branch, key = k:match "^([^.]+)%.(.+)"
	if branch then
		local tree = s[branch]
		if tree == nil then
			tree = {}
			s[branch] = tree
		elseif type(tree) ~= "table" then
			error ("Conflict setting key : " .. k)
		end
		s = tree
		k = key
	end
	
	if type(v) == "table" then
		local orig_v = s[k]
		if orig_v == nil then
			s[k] = v
		elseif type(orig_v) == "table" then
			for sub_k,sub_v in pairs(v) do
				patch(orig_v, sub_k, sub_v)
			end
		else
			error ("Conflict setting key : " .. k)
		end
	else
		s[k] = v
	end
end

local function settings_filename(filename)
	if filename then
		local realname = lfs.realpath(filename)
		local curpath, name = realname:match "(.*)[/\\]([^/\\]+)$"
		if curpath and name then
			lfs.chdir(curpath)
		end
		return name
	end
	if file.exist "main.game" then
		return "main.game"
	end
end

function S.init(args)
	local default_settings = datalist.parse(source.data.settingdefault)
	local realname = settings_filename(args[1])
	if realname then
		local game_settings = datalist.parse(file.load(realname))
		for k,v in pairs(game_settings) do
			patch(default_settings, k,v)
		end
	end
	for k,v in pairs(args) do
		if type(k) == "string" then
			if v == "true" then
				v = true
			elseif v == "false" then
				v = false
			else
				v = tonumber(v) or v
			end
			patch(default_settings, k,v)
		end
	end
	return default_settings
end

return S
