local render = require "soluna.render"
local defmat = require "soluna.material.default"

local ctx = ...
local state = ctx.state
local setting = ctx.settings
local inst_buffer = render.buffer {
	type = "vertex",
	usage = "stream",
	label = "texquad-instance",
	size = defmat.instance_size * setting.draw_instance,
}
local bindings = render.bindings()
bindings:vbuffer(0, inst_buffer)
bindings:view(0, state.views.storage)
bindings:sampler(0, state.default_sampler)

state.inst = assert(inst_buffer)
state.bindings = bindings
state.material = defmat.new {
	inst_buffer = state.inst,
	bindings = state.bindings,
	uniform = state.uniform,
	sr_buffer = state.srbuffer_mem,
	sprite_bank = ctx.arg.bank_ptr,
	tmp_buffer = ctx.tmp_buffer,
}

local material = {}

function material.reset()
	bindings:base(0)
end

function material.submit(ptr, n)
	state.material:submit(ptr, n)
end

function material.draw(ptr, n, tex)
	bindings:view(1, state.views[tex + 1])
	state.material:draw(ptr, n, tex)
end

return material
