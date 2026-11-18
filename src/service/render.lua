local ltask = require "ltask"
local render = require "soluna.render"
local image = require "soluna.image"
local embedsource = require "soluna.embedsource"
local drawmgr = require "soluna.drawmgr"
local file = require "soluna.file"

global require, assert, pairs, pcall, ipairs, print, load, type, tostring, print_r

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

local function find_material(materials, name)
	for _, v in pairs(materials) do
		if v.name == name then
			return v
		end
	end
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

	function batch.submit(id, ptr)
		local q = batch[id]
		local token = ltask.current_token()
		local function func()
			return ptr, token
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
	STATE.drawmgr:reset()
	for _, obj in pairs(STATE.materials) do
		obj.cobj:reset()
	end

	for i = 1, batch_n do
		local ptr = batch[i][1]()
		if ptr then
			STATE.drawmgr:append(ptr, STATE.pass)
		end
	end
	local materials = STATE.materials
	local views = STATE.views
	local from
	while true do
		local to, pass
		pass, from, to = STATE.drawmgr:nextpass(from)
		if pass == nil then
			break
		end
		for i = from , to do
			local mat, ptr, n, tex = STATE.drawmgr(i)
			materials[mat].cobj:submit(ptr, n)
		end
		from = to + 1
	end

	STATE.srbuffer:update(STATE.transient_buffer, STATE.srbuffer_mem:ptr())
	font.submit(STATE.font_texture)
	STATE.transient_buffer:copy()

	local from
	while true do
		local to, pass
		pass, from, to = STATE.drawmgr:nextpass(from)
		if pass == nil then
			break
		end
		render.begin_pass(pass)
		for i = from , to do
			local mat, ptr, n, tex = STATE.drawmgr(i)
			materials[mat].cobj:draw(ptr, n, tex and views[tex+1] or materials[mat].texture_view, pass)
		end
		render.end_pass(pass)
		from = to + 1
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
		local ptr, token = batch.consume(i)
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
			local _, token = resp()
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

	STATE = {
		pass = {
			-- 1. default pass
			render.pass { color0 = setting.background, swapchain = true },
		},
		default_sampler = render.sampler { label = "texquad-sampler", max_anisotropy = 16 },
		textures = {},
		font_texture = font_texture,
		views = views,
	}
	STATE.srbuffer = assert(sr_buffer)
	STATE.srbuffer_mem = render.srbuffer(setting.srbuffer_size)

	STATE.drawmgr = drawmgr.new(arg.bank_ptr, setting.draw_instance)

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
	local textmat = find_material(STATE.materials, "text")
	if textmat then
		textmat.set_texture_view( create_texture_view (font_texture, font.texture_size, font.texture_size) )
	end
end

function S.init(arg)
	ltask.mainthread_run(render_init, arg)
end

return S
