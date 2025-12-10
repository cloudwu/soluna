local image = require "soluna.image"
local file = require "soluna.file"
local datalist = require "soluna.datalist"

global type, tonumber, error, assert, ipairs, print

local M = {}

local function load_bundle(filename)
	local b = datalist.parse(file.load(filename))
	return b
end

local function crop_(item, c)
	local x = item.cx
	local y = item.cy
	local w = item.cw
	local h = item.ch
	
	local offx = item.x or 0
	local offy = item.y or 0
	if offx < 0 then
		offx = - c.w * offx // 1 | 0
	end
	if offy < 0 then
		offy = - c.h * offy // 1 | 0
	end
	local cx, cy, cw, ch = image.crop(c.data, c.w, c.h, x, y, w, h)
	offx = offx - cx
	offy = offy - cy
	
	item.x = offx
	item.y = offy
	item.cx = cx + (x or 0)
	item.cy = cy + (y or 0)
	item.cw = cw
	item.ch = ch
end

local function unpack_size(size)
	if type(size) == "number" then
		return size, size
	else
		local x, y = size:match "(%d+)[xX*](%d+)"
		return tonumber(x), tonumber(y)
	end
end

local function crop(item, filecache)
	local c = filecache[item.filename]
	if c == nil then
		error("No file : " .. item.filename)
	end
	local number = item.number
	if number then
		-- multi sprites
		local cw, ch = unpack_size(assert(item.size))
		local gap = item.gap
		local gap_x = 0
		local gap_y = 0
		if gap then
			gap_x, gap_y = unpack_size(gap)
		end
		local cx = 0
		local cy = 0
		local col = 1
		local row
		if type(number) == "number" then
			row = number
		else
			row, col = unpack_size(number)
		end
		local count = 1
		local offx = item.x
		local offy = item.y
		gap_x = gap_x + cw
		gap_y = gap_y + ch
		local filename = item.filename
		for i = 1, col do
			cx = 0
			for j = 1, row do
				local s = { cx = cx, cy = cy, cw = cw, ch = ch , x = offx , y = offy, filename = filename }
				item[count] = s
				crop_(s, c)
				count = count + 1
				cx = cx + gap_x
			end
			cy = cy + gap_y
		end
	else
		crop_(item, c)
	end
end

local function load_list(filecache, v, path)
	for idx, item in ipairs(v) do
		local fname = item.filename or "Need filename for item " .. idx
		if path then
			item.filename = path .. fname
		end
		crop(item, filecache)
	end
end

function M.load(filecache, filename, path)
	local v
	if type(filename) == "table" then
		v = filename
	else
		path = path or filename:match "(.*[/\\])[^/\\]+$"
		v = load_bundle(filename)
	end
	load_list(filecache, v, path)
	return v
end

function M.loadimage(filecache, filename)
	local content = file.load(filename)
	if not content then
		if not filecache.__missing[filename] then
			filecache.__missing[filename] = true
			print("Missing file : " .. filename)
		end
		return
	end
	local load = image.load
	if filename:find "%.alpha%." then
		load = image.load_alpha
	end
	local data, w, h = load(content)
	if data == nil then
		if not filecache.__missing[filename] then
			filecache.__missing[filename] = true
			print("Invalid image : " .. filename .. "(" .. w .. ")")
		end
		return
	end
	local r = { data = data, w = w, h = h }
	filecache[filename] = r
	return r
end

return M
