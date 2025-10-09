local sdf = require "soluna.image.sdf"
local datalist = require "soluna.datalist"
local file = require "soluna.file"
local mattext = require "soluna.material.text"

global error, tostring, print

local icon = {}

function icon.bundle(filename)
	local path = filename:match "(.*[/\\])[^/\\]+$"
	local b = datalist.parse(file.load(filename))
	local names = {}
	local icons = {}
	local n = #b
	for i = 1, n do
		local icon = b[i]
		names[icon.name] = i - 1
		local src = file.load(path .. icon.image) or error ("Open icon fail : " .. tostring(icon.name))
		local img = sdf.load(src)
		icons[i] = img
	end
	icon.names = names
	return sdf.bundle(icons)
end

function icon.symbol(name, size, color)
	local id = icon.names[name] or error "No icon " .. name
	return mattext.char(id, 255, size, color)
end

return icon