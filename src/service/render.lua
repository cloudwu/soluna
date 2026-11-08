local ltask = require "ltask"
local render = require "soluna.render"
local image = require "soluna.image"
local embedsource = require "soluna.embedsource"
local drawmgr = require "soluna.drawmgr"
local mattarget = require "soluna.material.target"
local file = require "soluna.file"

global require, assert, pairs, pcall, ipairs, print, load, type

local setting = require "soluna".settings()

local font = {}

local function create_materials(ctx)
	local materials = {}

	local function load_material(source, chunkname, id)
		local chunk = assert(load(source, chunkname))
		local mctx = {
			id = id,
			state = ctx.state,
			arg = ctx.arg,
			tmp_buffer = ctx.tmp_buffer,
			settings = ctx.settings,
			font = ctx.font,
			render = ctx.render,
		}
		local material = assert(chunk(mctx), chunkname .. " : no material returned")
		assert(type(material.submit) == "function", chunkname .. " : missing submit function")
		assert(type(material.draw) == "function", chunkname .. " : missing draw function")
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
			img:update(texture_ptr)
		end
	end
end

local batch = {}; do
	local thread
	local submit_n = 0
	function batch.register(addr)
		local n = #batch + 1
		batch[n] = {
			source = addr
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

	function batch.submit(id, ptr, size)
		local q = batch[id]
		local token = ltask.current_token()
		local function func()
			return ptr, size, token
		end
		if q[1] == nil then
			submit_n = submit_n + 1
			if thread and submit_n == #batch then
				ltask.wakeup(thread)
				thread = nil
			end
			q[1] = func
		else
			q[#q + 1] = func
		end
		ltask.wait()
	end

	function batch.consume(id)
		local q = batch[id]
		local r = assert(q[1])
		local n = #q
		for i = 1, n - 1 do
			q[i] = q[i + 1]
		end
		q[n] = nil
		return r()
	end
end

local STATE

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
				STATE.views[tid] = render.view { texture = tex }
			end
			tex:update(imgmem[i])
		end
		update_image = nil
	end
end

local new_targets = {}
local screen = {}
local passes = {}
local stack = {}

local function new_target(id, texid, w, h, color)
	local img = render.image { width = w, height = h, color_attachment = true }
	local attachment = {
		color0 = render.view { color_attachment = img },
		depth_stencil = render.view {
			depth_stencil_attachment = render.image { width = w, height = h, depth_stencil_attachment = true },
		},
	}
	STATE.textures[texid + 1] = img
	STATE.views[texid + 1] = render.view { texture = img }
	STATE.texsize[texid] = { 1 / w, 1 / h }
	STATE.targets[id] = {
		attachment = attachment,
		clear = render.pass { color0 = color or "clear", attachment = attachment },
		load = render.pass { color0 = "load", attachment = attachment },
		framesize = { 2 / w, (render.origin_top_left() and -2 or 2) / h },
	}
end

local function frame(count)
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
	STATE.drawmgr:reset()
	for _, obj in pairs(STATE.materials) do
		if obj.reset then
			obj.reset()
		end
	end

	for i = 1, batch_n do
		local ptr, size = batch[i][1]()
		if ptr then
			STATE.drawmgr:append(ptr, size)
		end
	end
	local npass = 0
	local nstack = 0
	local list = screen
	screen.n = 0
	for i = 1, #STATE.drawmgr do
		local mat, ptr, n = STATE.drawmgr(i)
		if mat == STATE.target_material then
			for j = 0, n - 1 do
				local id = mattarget.sprite(ptr, j)
				if id > 0 then
					nstack = nstack + 1
					stack[nstack] = assert(STATE.targets[id])
				elseif nstack > 0 then
					nstack = nstack - 1
				end
			end
			if nstack > 0 then
				npass = npass + 1
				list = passes[npass]
				if not list then
					list = {}
					passes[npass] = list
				end
				list.target = stack[nstack]
				list.n = 0
			else
				list = screen
			end
		else
			local k = list.n + 1
			list.n = k
			list[k] = i
		end
	end
	for p = 1, npass + 1 do
		local list = p > npass and screen or passes[p]
		for k = 1, list.n do
			local mat, ptr, n = STATE.drawmgr(list[k])
			local obj = assert(STATE.materials[mat])
			obj.submit(ptr, n)
		end
	end
	STATE.srbuffer:update(STATE.srbuffer_mem:ptr())
	font.submit(STATE.font_texture)
	for p = 1, npass + 1 do
		local list = p > npass and screen or passes[p]
		local target = list.target
		local pass = STATE.pass
		if target then
			if target.frame == count then
				pass = target.load
			else
				pass = target.clear
				target.frame = count
			end
			STATE.uniform.framesize = target.framesize
		else
			STATE.uniform.framesize = STATE.framesize
		end
		pass:begin()
		for k = 1, list.n do
			local mat, ptr, n, tex = STATE.drawmgr(list[k])
			STATE.uniform.tex_size = STATE.texsize[tex] or STATE.atlas_texsize
			STATE.materials[mat].draw(ptr, n, tex)
		end
		pass:finish()
	end
	render.submit()
end

function S.frame(count)
	batch.wait()
	local ok, err = pcall(ltask.mainthread_run, frame, count)
	if not ok then
		print("RENDER ERR", err)
	end
	for i = 1, #batch do
		local ptr, size, token = batch.consume(i)
		ltask.wakeup(token)
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
		for _, resp in ipairs(v) do
			local _, _, token = resp()
			ltask.wakeup(token)
		end
	end

	for addr in pairs(workers) do
		ltask.call(addr, "quit")
	end
	font.shutdown()
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

function S.target(w, h, color)
	local loader = ltask.uniqueservice "loader"
	local id, texid = ltask.call(loader, "target", w, h)
	new_targets[#new_targets + 1] = function()
		new_target(id, texid, w, h, color)
	end
	return id
end

local function render_init(arg)
	font.init()

	local texture_size = setting.texture_size
	local sr_buffer = render.buffer {
		type = "storage",
		usage = "dynamic",
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
		font = render.view { texture = font_texture },
	}

	local sprite_sampler = setting.sprite_sampler
	sprite_sampler.label = sprite_sampler.label or "texquad-sampler"

	STATE = {
		pass = render.pass {
			color0 = setting.background,
			swapchain = true,
		},
		default_sampler = render.sampler(sprite_sampler),
		textures = {},
		font_texture = font_texture,
		views = views,
	}
	STATE.srbuffer = assert(sr_buffer)
	STATE.srbuffer_mem = render.srbuffer(setting.srbuffer_size)

	STATE.drawmgr = drawmgr.new(arg.bank_ptr, setting.draw_instance)

	STATE.uniform = render.uniform {
		16, -- size
		framesize = {
			offset = 0,
			type = "float",
			n = 2,
		},
		tex_size = {
			offset = 8,
			type = "float",
			n = 2,
		},
	}
	STATE.framesize = { 2 / arg.width, -2 / arg.height }
	STATE.atlas_texsize = { 1 / texture_size, 1 / texture_size }
	STATE.texsize = {}
	STATE.targets = {}

	local tmp_buffer = render.tmp_buffer(setting.tmpbuffer_size)
	STATE.materials = create_materials {
		state = STATE,
		arg = arg,
		tmp_buffer = tmp_buffer,
		settings = setting,
		font = font,
		render = render,
	}
end

function S.init(arg)
	ltask.mainthread_run(render_init, arg)
end

function S.resize(w, h)
	STATE.framesize = { 2 / w, -2 / h }
end

return S
