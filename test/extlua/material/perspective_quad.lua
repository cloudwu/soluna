local render = require "soluna.render"
local pqmat = require "ext.material.perspective_quad"

local ctx = ...
local state = ctx.state

pqmat.set_material_id(ctx.id)

local inst_buffer = render.buffer {
	type = "vertex",
	usage = "stream",
	label = "extlua-perspective-quad-instance",
	size = pqmat.instance_size * ctx.settings.draw_instance,
}

local bindings = render.bindings()
bindings:vbuffer(0, inst_buffer)
bindings:sampler(0, state.default_sampler)

local cobj = pqmat.new {
	inst_buffer = inst_buffer,
	bindings = bindings,
	uniform = state.uniform,
	sprite_bank = ctx.arg.bank_ptr,
	tmp_buffer = ctx.tmp_buffer,
}

local material = {}

function material.reset()
	cobj:reset()
end

function material.submit(ptr, n)
	cobj:submit(ptr, n)
end

function material.draw(ptr, n, tex)
	bindings:view(1, state.views[tex + 1])
	cobj:draw(ptr, n, tex)
end

return material
