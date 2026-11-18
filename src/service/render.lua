local ltask = require "ltask"
local render = require "soluna.render"
local image = require "soluna.image"
local embedsource = require "soluna.embedsource"
local drawmgr = require "soluna.drawmgr"
local file = require "soluna.file"

global require, assert, pairs, pcall, ipairs, print, load, type, tostring, print_r, table

local setting = require "soluna".settings()
local STATE
local font = {}

local function create_texture_view (img, width, height)
	return render.texture_view {
		width = width,
		height = height,
		view = render.view { texture = img }
	}
end

local function create_materials(ctx)
	local materials = {}
	local function load_material(source, chunkname, id)
		local chunk = assert(load(source, chunkname))
		local mctx = {
			id = id,
			bank_ptr = ctx.arg.bank_ptr,
			views = ctx.views,
			settings = ctx.settings,
			font = ctx.font,
			render = ctx.render,
			transient_buffer = ctx.transient_buffer,
			default_sampler = ctx.default_sampler,
			sr_buffer = ctx.sr_buffer,
		}
		local material = assert(chunk(mctx), chunkname .. " : no material returned")
		materials[id] = material
	end

	local MATERIAL_EXTLUA_BASE <const> = 256
	do
		local next_id = 0
		for _, name in ipairs(embedsource.material) do
			local id = next_id
			assert(id < MATERIAL_EXTLUA_BASE)
			next_id = id + 1
			local loader = assert(embedsource.material[name])
			load_material(loader(), "@src/material/" .. name .. ".lua", id)
		end
	end
	if setting.extlua_material then
		local list = setting.extlua_material
		if type(list) == "string" then
			list = { list }
		end
		local path = assert(setting.extlua_material_path)
		local next_id = MATERIAL_EXTLUA_BASE
		for _, name in ipairs(list) do
			local id = next_id
			next_id = id + 1
			local fullname = assert(file.searchpath(name, path))
			load_material(file.load(fullname), "@" .. fullname, id)
		end
	end
	return materials
end

do
	local mgr = require "soluna.font.manager"
	local fontapi = require "soluna.font"
	local texture_ptr

	function font.init()
		mgr.init(embedsource.runtime.fontmgr(), "@src/lualib/fontmgr.lua")
		font.texture_size = fontapi.texture_size
		font.cobj = fontapi.cobj()
		texture_ptr = fontapi.texture()
	end

	function font.shutdown()
		mgr.shutdown()
	end

	function font.submit(img)
		if fontapi.submit() then
			img:update(STATE.transient_buffer, texture_ptr)
		end
	end
end

local batch = {}; do
	local thread
	local submit_n = 0
	function batch.register(addr)
		local n = #batch + 1
		batch[n] = {
			source = addr,
			tokens = {},
		}
		return n
	end

	function batch.wait()
		if submit_n ~= #batch then
			thread = ltask.current_token()
			ltask.wait()
		end
		submit_n = 0
	end

	function batch.submit(id, ptr)
		local q = batch[id]
		local token = ltask.current_token()
		if q[1] == nil then
			submit_n = submit_n + 1
			if thread and submit_n == #batch then
				ltask.wakeup(thread)
				thread = nil
			end
			q[1] = ptr
			q.tokens[1] = token
		else
			local n = #q + 1
			q[n] = ptr
			q.tokens[n] = token
		end
		ltask.wait()
	end

	function batch.consume(id)
		local q = batch[id]
		local token = table.remove(q.tokens, 1)
		table.remove(q, 1)
		return token
	end
end

local S = {}

function S.app(settings)
	local soluna_app = require "soluna.app"
	for k, v in pairs(settings) do
		local f = soluna_app[k]
		if f then
			f(v)
		end
	end
end

-- todo: update mutiple images
local update_image
local new_targets = {}

local function delay_update_image(imgmem)
	function update_image()
		local from = imgmem.from
		for i = 1, #imgmem do
			local tid = from + i
			local tex = STATE.textures[tid]
			if tex == nil then
				local texture_size = setting.texture_size
				tex = render.image {
					width = texture_size,
					height = texture_size,
				}
				STATE.textures[tid] = tex
				STATE.views[tid] = create_texture_view(tex, texture_size, texture_size)
			end
			tex:update(STATE.transient_buffer, imgmem[i])
		end
		update_image = nil
	end
end

local function frame(count)
	STATE.transient_buffer:reset()

	local batch_size = setting.batch_size

	-- todo: do not wait all batch commits
	local batch_n = #batch
	if update_image then update_image() end
	if new_targets[1] then
		for _, f in ipairs(new_targets) do
			f()
		end
		new_targets = {}
	end
	local mgr = STATE.drawmgr
	mgr:reset()
	for _, mat in pairs(STATE.materials_cobj) do
		mat:reset()
	end

	for i = 1, batch_n do
		local ptr = batch[i][1]
		if ptr then
			mgr:append(ptr, STATE.pass)
		end
	end
	mgr:sortswapchain()
	local materials = STATE.materials_cobj
	local views = STATE.views
	local from
	while true do
		local to, pass
		pass, from, to = mgr:nextpass(from)
		if pass == nil then
			break
		end
		for i = from , to do
			local mat, ptr, n, tex = mgr(i)
			materials[mat]:submit(ptr, n)
		end
		from = to + 1
	end

	STATE.srbuffer:update(STATE.transient_buffer, STATE.srbuffer_mem:ptr())
	font.submit(STATE.font_texture)
	STATE.transient_buffer:copy()
	local from
	while true do
		local to, pass
		pass, from, to = mgr:nextpass(from)
		if pass == nil then
			break
		end
		mgr:beginpass(from)
		for i = from , to do
			local mat_id, ptr, n, tex = mgr(i)
			local mat = materials[mat_id]
			mat:draw(ptr, n, tex and views[tex+1] or views[mat], pass)
		end
		mgr:endpass(from)
		from = to + 1
	end
	mgr:submit()
end

function S.frame(count)
	batch.wait()
	local ok, err = pcall(ltask.mainthread_run, frame, count)
	if not ok then
		print("RENDER ERR", err)
	end
	for i = 1, #batch do
		local token = batch.consume(i)
		if token then
			ltask.wakeup(token)
		end
	end
	assert(ok, err)
end

S.register_batch = assert(batch.register)
S.submit_batch = assert(batch.submit)

function S.quit()
	local workers = {}
	for _, v in ipairs(batch) do
		workers[v.source] = true
	end

	S.submit_batch = function() end -- prevent submit

	for _, v in ipairs(batch) do
		for _, token in ipairs(v.tokens) do
			ltask.wakeup(token)
		end
	end

	for addr in pairs(workers) do
		ltask.call(addr, "quit")
	end
	font.shutdown()
end

local function new_target(id, texid, w, h, color)
	local img = render.image { width = w, height = h, color_attachment = true }
	STATE.textures[texid + 1] = img
	STATE.views[texid + 1] = render.texture_view {
		width = w,
		height = h,
		view = render.view { texture = img },
		premultiplied = true,
	}
	STATE.pass[id] = STATE.drawmgr:newpass {
		color0 = color or "clear",
		attachment = { color0 = render.view { color_attachment = img } },
		width = w,
		height = h,
	}
end

function S.target(w, h, color)
	local loader = ltask.uniqueservice "loader"
	local sprite, texid = ltask.call(loader, "target", w, h)
	STATE.pass_n = STATE.pass_n + 1
	local id = STATE.pass_n
	new_targets[#new_targets + 1] = function()
		new_target(id, texid, w, h, color)
	end
	return id, sprite
end

function S.load_sprites(name)
	local loader = ltask.uniqueservice "loader"
	local spr = ltask.call(loader, "loadbundle", name)
	local rects, from = ltask.call(loader, "pack")
	local imgmems = { from = from }
	for i = 1, #rects do
		local imgmem = image.new(setting.texture_size, setting.texture_size)
		local canvas = imgmem:canvas()
		for id, v in pairs(rects[i]) do
			local src = image.canvas(v.data, v.w, v.h, v.stride)
			image.blit(canvas, src, v.x, v.y)
		end
		imgmems[i] = imgmem
	end
	delay_update_image(imgmems)
	return spr
end

local function render_init(arg)
	font.init()

	local texture_size = setting.texture_size
	
	local sr_buffer = render.buffer {
		type = "storage",
		label = "texquad-scalerot",
		size = render.buffer_size("srbuffer", setting.srbuffer_size),
	}
	-- todo: don't load texture here

	local font_texture = render.image {
		width = font.texture_size,
		height = font.texture_size,
		pixel_format = "R8",
	}
	local views = {
		storage = render.view { storage = sr_buffer },
	}

	local mgr = drawmgr.new(arg.bank_ptr, setting.draw_instance)
	local sprite_sampler = setting.sprite_sampler
	sprite_sampler.label = sprite_sampler.label or "texquad-sampler"

	STATE = {
		pass = {
			-- 1. swapchain pass
			mgr:newpass { color0 = setting.background, swapchain = true },
		},
		pass_n = 1,
		default_sampler = render.sampler(sprite_sampler),
		textures = {},
		font_texture = font_texture,
		views = views,
		drawmgr = mgr,
	}
	STATE.srbuffer = assert(sr_buffer)
	STATE.srbuffer_mem = render.srbuffer(setting.srbuffer_size)

	STATE.transient_buffer = render.transient_buffer(setting.transient_buffer, setting.tmpbuffer_size)
	STATE.materials = create_materials {
		state = STATE,
		arg = arg,
		settings = setting,
		font = font,
		render = render,
		transient_buffer = STATE.transient_buffer,
		views = views,
		sr_buffer = STATE.srbuffer_mem,
		default_sampler = STATE.default_sampler,
	}
	local materials_cobj = {}
	STATE.materials_cobj = materials_cobj
	for k,v in pairs(STATE.materials) do
		materials_cobj[k] = v.cobj
		if v.name == "text" then
			views[v.cobj] = create_texture_view (font_texture, font.texture_size, font.texture_size)
		end
	end
end

function S.init(arg)
	ltask.mainthread_run(render_init, arg)
end

return S
