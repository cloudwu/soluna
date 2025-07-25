local yoga = require "soluna.layout.yoga"
local datalist = require "soluna.datalist"
local file = require "soluna.file"

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

-- update attr
function element:update(attr)
	local cobj = (self._yoga and self._yoga[self._id]) or error ("No id : " .. self._id)
	yoga.node_set(cobj, attr)
end

do
	local function parse_node(v)
		local attr = {}
		local content = {}
		local n = 1
		for i = 1, #v, 2 do
			local name = v[i]
			local value = v[i+1]
			if type(value) == "table" then
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
		if attr.image or attr.text then
			local obj = {
				image = attr.image,
				text = attr.text,
				size = attr.size,
				color = attr.color,
				align = attr.text_align,
			}
			doc._yoga[obj] = cobj
			doc._list[#doc._list + 1] = obj
		end
	end

	local function add_children(doc, parent, list)
		for i = 1, #list, 2 do
			local name = list[i]	-- ignore
			local content, attr = parse_node(list[i+1])
			local cobj = yoga.node_new(parent)
			new_element(doc, cobj, attr)
			if content then
				add_children(doc, cobj, content)
			end
		end
	end

	function layout.load(filename)
		local list = datalist.parse_list(file.loader(filename))
		local doc = {
			_root = yoga.node_new(),
			_yoga = {},
			_list = {},
			_element = {},
		}
		
		local children, attr = parse_node(list)
		new_element(doc, doc._root, attr)
		if children then
			add_children(doc, doc._root, children)
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
				x,y,w,h = yoga.node_get(cobj)
			end
		end
		return list
	end
end

return layout
