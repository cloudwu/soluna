local ltask = require "ltask"
local render = require "soluna.render"
local image = require "soluna.image"
local embedsource = require "soluna.embedsource"
local drawmgr = require "soluna.drawmgr"
local defmat = require "soluna.material.default"
local textmat = require "soluna.material.text"

local setting = require "soluna".settings()

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

local S = {}

local function frame(count)
	local batch_size = setting.batch_size

	-- todo: do not wait all batch commits
	local batch_n = #batch
	batch.wait()
	STATE.drawmgr:reset()
	STATE.bindings:voffset(0, 0)
	STATE.uniform.baseinst = 0
	for i = 1, batch_n do
		local ptr, size = batch[i][1]()
		if ptr then
			STATE.drawmgr:append(ptr, size)
		end
	end
	local draw_n = #STATE.drawmgr
	for i = 1, draw_n do
		local mat, ptr, n, tex = STATE.drawmgr(i)
		if mat == 0 then
			assert(tex == 0)
			STATE.material:submit(ptr, n)
		else
			assert(mat == 1)
			-- text
			STATE.material_text:submit(ptr, n)
		end
	end
	STATE.srbuffer:update(STATE.srbuffer_mem:ptr())
	STATE.pass:begin()
	font.submit(STATE.font_texture)
		for i = 1, draw_n do
			local mat, ptr, n, tex = STATE.drawmgr(i)
			if mat == 0 then
				STATE.bindings:image(0, STATE.textures[tex+1])
				STATE.material:draw(ptr, n, tex)
			else
				assert(mat == 1)
				STATE.bindings:image(0, STATE.font_texture)
				STATE.material_text:draw(ptr, n)
			end
		end
	STATE.pass:finish()
end

function S.frame(count)
	local ok , err = pcall(frame, count)
	render.submit()
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
	for addr in pairs(workers) do
		ltask.call(addr, "quit")
	end
	for _, v in ipairs(batch) do
		for _, resp in ipairs(v) do
			local _,_, token = resp()
			ltask.wakeup(token)
		end
	end
	-- double check
	for addr in pairs(workers) do
		ltask.call(addr, "quit")
	end
	font.shutdown()
end

function S.load_sprites(name)
	local loader = ltask.uniqueservice "loader"
	local spr = ltask.call(loader, "loadbundle", name)
	local rect = ltask.call(loader, "pack")

	local imgmem = image.new(setting.texture_size, setting.texture_size)
	local canvas = imgmem:canvas()
	for id, v in pairs(rect) do
		local src = image.canvas(v.data, v.w, v.h, v.stride)
		image.blit(canvas, src, v.x, v.y)
	end
	
	STATE.textures[1]:update(imgmem)
end

function S.init(arg)
	font.init()
	local loader = ltask.uniqueservice "loader"

	local texture_size = setting.texture_size
	
	local img = render.image {
		width = texture_size,
		height = texture_size,
	}
	
	local inst_buffer = render.buffer {
		type = "vertex",
		usage = "stream",
		label = "texquad-instance",
		size = render.buffer_size("inst", setting.draw_instance),
	}
	local sr_buffer = render.buffer {
		type = "storage",
		usage = "dynamic",
		label = "texquad-scalerot",
		size = render.buffer_size("srbuffer", setting.srbuffer_size),
	}
	local sprite_buffer = render.buffer {
		type = "storage",
		usage = "stream",
		label =  "texquad-sprite",
		size = render.buffer_size("sprite", setting.draw_instance),
	}

	-- todo: don't load texture here
	
	local bank_ptr = ltask.call(loader, "init", {
		max_sprite = setting.sprite_max,
		texture_size = texture_size,
	})

	STATE = {
		pass = render.pass {
			color0 = 0x4080c0,
		},
	}
	local bindings = render.bindings()
	bindings:vbuffer(0, inst_buffer)
	bindings:sbuffer(0, sr_buffer)
	bindings:sbuffer(1, sprite_buffer)
	bindings:image(0, img)
	bindings:sampler(0, render.sampler { label = "texquad-sampler" })	-- todo : ref this sampler
	
	STATE.textures = { img }
	STATE.font_texture = render.image {
		width = font.texture_size,
		height = font.texture_size,
		pixel_format = "R8",
	}
	
	STATE.inst = assert(inst_buffer)
	STATE.srbuffer = assert(sr_buffer)
	STATE.sprite = assert(sprite_buffer)

	STATE.srbuffer_mem = render.srbuffer(setting.srbuffer_size)
	STATE.bindings = bindings
	
	STATE.drawmgr = drawmgr.new(bank_ptr, setting.draw_instance)
	
	STATE.uniform = render.uniform {
		16,	-- size
		tex_size = {
			offset = 0,
			type = "float",
		},
		baseinst = {
			offset = 4,
			type = "int",
		},
		framesize = {
			offset = 8,
			type = "float",
			n = 2,
		},
	}
	STATE.uniform.framesize = { 2/arg.width, -2/arg.height }
	STATE.uniform.tex_size = 1/texture_size
	STATE.uniform.baseinst = 0

	STATE.material = defmat.new {
		inst_buffer = STATE.inst,
		sprite_buffer = STATE.sprite,
		bindings = STATE.bindings,
		uniform = STATE.uniform,
		sr_buffer = STATE.srbuffer_mem,
		sprite_bank = bank_ptr,
	}
	
	STATE.material_text = textmat.normal {
		inst_buffer = STATE.inst,
		sprite_buffer = STATE.sprite,
		bindings = STATE.bindings,
		uniform = STATE.uniform,
		sr_buffer = STATE.srbuffer_mem,
		font_manager = font.cobj,
	}
end

return S
