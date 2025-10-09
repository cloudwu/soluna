local yoga = require "soluna.layout.yoga"
local datalist = require "soluna.datalist"
local file = require "soluna.file"
local table = table

global next, error, assert, type, setmetatable, pairs

local layout = {}

local document = {}
local element = {} ; element.__index = element

function document:__gc()
	local root = self._root	-- root yoga object
	if root then
		yoga.node_free(root)
		self._root = nil
	end
	self._yoga = nil	-- yoga objects for elements
	self._list = nil	-- image/text element lists
	self._element = nil	-- elements can be update
end

function document:__index(id)
	return self._element[id]
end

function document:__tostring()
	return "[document]"
end

function document:__pairs()
	return next, self._element
end

function element:__tostring()
	return "[element:"..self._id.."]"
end

function element:__newindex(key, value)
	local _yoga = self._document._yoga
	local cobj = (_yoga and _yoga[self._id]) or error ("No id : " .. self._id)
	yoga.node_set(cobj, key, value)
end

-- update attr
function element:update(attr)
	local _yoga = self._document._yoga
	local cobj = (_yoga and _yoga[self._id]) or error ("No id : " .. self._id)
	yoga.node_set(cobj, attr)
end

function element:get()
	local _yoga = self._document._yoga
	local cobj = (_yoga and _yoga[self._id]) or error ("No id : " .. self._id)
	return yoga.node_get(cobj)
end

function element:attribs()
	local _yoga = self._document._yoga
	local cobj = (_yoga and _yoga[self._id]) or error ("No id : " .. self._id)
	return _yoga[cobj]
end

do
	local function parse_node(v, scripts)
		local attr = {}
		local content = {}
		local n = 1
		for i = 1, #v, 2 do
			local name = v[i]
			local value = v[i+1]
			if name == "children" then
				local c = scripts(value)
				local len = #c
				assert(type(c) == "table" and len % 2 == 0)
				table.move(c, 1, len, n, content)
				n = n + len
			elseif type(value) == "table" then
				content[n] = name
				content[n+1] = value
				n = n + 2
			else
				attr[name] = value
			end
		end
		if n == 1 then
			content = nil
		end
		return content, attr
	end
	
	local function new_element(doc, cobj, attr)
		yoga.node_set(cobj, attr)
		local id = attr.id
		if id then
			if doc._element[id] then
				error (id .. " exist")
			end
			local elem = { _document = doc, _id = id }
			doc._element[id] = setmetatable(elem, element)
			doc._yoga[id] = cobj
		end
		
		if attr.image or attr.text or attr.background or attr.region then
			local obj = {}
			for k,v in pairs(attr) do
				obj[k] = v
			end
			doc._yoga[obj] = cobj
			doc._yoga[cobj] = obj
			doc._list[#doc._list + 1] = obj
		end
	end

	local function add_children(doc, parent, list, scripts)
		for i = 1, #list, 2 do
			local name = list[i]	-- ignore
			local content, attr = parse_node(list[i+1], scripts)
			local cobj = yoga.node_new(parent)
			new_element(doc, cobj, attr)
			if content then
				add_children(doc, cobj, content, scripts)
			end
		end
	end

	function layout.load(filename_or_list, scripts)
		local list
		if type(filename_or_list) == "string" then
			list = datalist.parse_list(file.load(filename_or_list))
		else
			list = filename_or_list
		end
		local doc = {
			_root = yoga.node_new(),
			_yoga = {},
			_list = {},
			_element = {},
		}
		
		local children, attr = parse_node(list, scripts)
		new_element(doc, doc._root, attr)
		if children then
			add_children(doc, doc._root, children, scripts)
		end

		return setmetatable(doc, document)
	end
	
	function layout.calc(doc)
		yoga.node_calc(doc._root)
		local list = doc._list
		local yogaobj = doc._yoga
		for i = 1, #list do
			local obj = list[i]
			local cobj = yogaobj[obj]
			do local _ENV = obj
				global x, y, w, h
				x,y,w,h = yoga.node_get(cobj)
			end
		end
		return list
	end
end

return layout
