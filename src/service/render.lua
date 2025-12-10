local ltask = require "ltask"
local render = require "soluna.render"
local image = require "soluna.image"
local embedsource = require "soluna.embedsource"
local drawmgr = require "soluna.drawmgr"
local defmat = require "soluna.material.default"
local textmat = require "soluna.material.text"
local quadmat = require "soluna.material.quad"
local maskmat = require "soluna.material.mask"
local soluna_app = require "soluna.app"

global require, assert, pairs, pcall, ipairs, print

local setting = require "soluna".settings()

local DEFAULT_MAT <const> = 0
local TEXT_MAT <const> = 1
local QUAD_MAT <const> = 2
local MASK_MAT <const> = 3

local font = {} ;  do
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

local batch = {} ; do
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
		local function func ()
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
			q[#q+1] = func
		end
		ltask.wait()
	end
	function batch.consume(id)
		local q = batch[id]
		local r = assert(q[1])
		local n = #q
		for i = 1, n - 1 do
			q[i] = q[i+1]
		end
		q[n] = nil
		return r()
	end
end

local STATE

local materials = {
	[DEFAULT_MAT] = {
		submit = function(ptr, n)
			STATE.material:submit(ptr, n)
		end,
		draw = function(ptr, n, tex)
			STATE.bindings:view(1, STATE.views[tex+1])
			STATE.material:draw(ptr, n, tex)
		end,
	},
	[TEXT_MAT] = {
		submit = function(ptr, n)
			STATE.material_text:submit(ptr, n)
		end,
		draw = function(ptr, n)
			STATE.bindings:view(1, STATE.views.font)
			STATE.material_text:draw(ptr, n)
		end,
	},
	[QUAD_MAT] = {
		submit = function(ptr, n)
			STATE.material_quad:submit(ptr, n)
		end,
		draw = function(ptr, n)
			STATE.material_quad:draw(ptr, n)
		end,
	},
	[MASK_MAT] = {
		submit = function(ptr, n)
			STATE.material_mask:submit(ptr, n)
		end,
		draw = function(ptr, n, tex)
			STATE.mask_bindings:view(1, STATE.views[tex+1])
			STATE.material_mask:draw(ptr, n, tex)
		end,
	}
}


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

local function frame(count)
	local batch_size = setting.batch_size

	-- todo: do not wait all batch commits
	local batch_n = #batch
	if update_image then update_image() end
	STATE.drawmgr:reset()
	STATE.bindings:base(0)
	STATE.quad_bindings:base(0)
	STATE.mask_bindings:base(0)
	for i = 1, batch_n do
		local ptr, size = batch[i][1]()
		if ptr then
			STATE.drawmgr:append(ptr, size)
		end
	end
	local draw_n = #STATE.drawmgr
	for i = 1, draw_n do
		local mat, ptr, n, tex = STATE.drawmgr(i)
		local obj = materials[mat]
		obj.submit(ptr, n)
	end
	STATE.srbuffer:update(STATE.srbuffer_mem:ptr())
	STATE.pass:begin()
		font.submit(STATE.font_texture)
		for i = 1, draw_n do
			local mat, ptr, n, tex = STATE.drawmgr(i)
			local obj = materials[mat]
			obj.draw(ptr, n, tex)
		end
	STATE.pass:finish()
	render.submit()
end

function S.frame(count)
	batch.wait()
	local ok , err = pcall(ltask.mainthread_run, frame, count)
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
	
	S.submit_batch = function() end	-- prevent submit

	for _, v in ipairs(batch) do
		for _, resp in ipairs(v) do
			local _,_, token = resp()
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

	local inst_buffer = render.buffer {
		type = "vertex",
		usage = "stream",
		label = "texquad-instance",
		size = defmat.instance_size * setting.draw_instance,	-- textmat.instance_size is the same
	}
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

	STATE = {
		pass = render.pass {
			color0 = setting.background,
			swapchain = true,
		},
		default_sampler = render.sampler { label = "texquad-sampler" },
		textures = {},
		font_texture = font_texture,
		views = views,
	}
	local bindings = render.bindings()
	bindings:vbuffer(0, inst_buffer)
	bindings:view(0, views.storage)
	bindings:sampler(0, STATE.default_sampler)
	
	STATE.inst = assert(inst_buffer)
	STATE.srbuffer = assert(sr_buffer)

	STATE.srbuffer_mem = render.srbuffer(setting.srbuffer_size)
	STATE.bindings = bindings

	do
		STATE.quad_inst = render.buffer {
			type = "vertex",
			usage = "stream",
			label = "quad-instance",
			size = quadmat.instance_size * setting.draw_instance,
		}

		local quadbind = render.bindings()
		quadbind:vbuffer(0, STATE.quad_inst)
		quadbind:view(0, views.storage)

		STATE.quad_bindings = quadbind
	end
	
	do
		STATE.mask_inst = render.buffer {
			type = "vertex",
			usage = "stream",
			label = "mask-instance",
			size = maskmat.instance_size * setting.draw_instance,
		}

		local maskbind = render.bindings()

		maskbind:vbuffer(0, STATE.mask_inst)
		maskbind:view(0, views.storage)
		maskbind:sampler(0, STATE.default_sampler)
		 		
		STATE.mask_bindings = maskbind
	end
	
	STATE.drawmgr = drawmgr.new(arg.bank_ptr, setting.draw_instance)
	
	STATE.uniform = render.uniform {
		12,	-- size
		framesize = {
			offset = 0,
			type = "float",
			n = 2,
		},
		tex_size = {
			offset = 8,
			type = "float",
		},
	}
	STATE.uniform.framesize = { 2/arg.width, -2/arg.height }
	STATE.uniform.tex_size = 1/texture_size
	
	local tmp_buffer = render.tmp_buffer(setting.tmpbuffer_size)

	STATE.material = defmat.new {
		inst_buffer = STATE.inst,
		bindings = STATE.bindings,
		uniform = STATE.uniform,
		sr_buffer = STATE.srbuffer_mem,
		sprite_bank = arg.bank_ptr,
		tmp_buffer = tmp_buffer,
	}

	STATE.material_mask = maskmat.new {
		inst_buffer = STATE.mask_inst,
		bindings = STATE.mask_bindings,
		uniform = STATE.uniform,
		sr_buffer = STATE.srbuffer_mem,
		sprite_bank = arg.bank_ptr,
		tmp_buffer = tmp_buffer,
	}
	
	STATE.material_text = textmat.normal {
		inst_buffer = STATE.inst,
		bindings = STATE.bindings,
		uniform = STATE.uniform,
		sr_buffer = STATE.srbuffer_mem,
		font_manager = font.cobj,
		tmp_buffer = tmp_buffer,
	}

	STATE.material_quad = quadmat.new {
		inst_buffer = STATE.quad_inst,
		bindings = STATE.quad_bindings,
		uniform = STATE.uniform,
		sr_buffer = STATE.srbuffer_mem,
		tmp_buffer = tmp_buffer,
	}
end

function S.init(arg)
	ltask.mainthread_run(render_init, arg)
end

function S.resize(w, h)
	STATE.uniform.framesize = { 2/w, -2/h }
end

return S
