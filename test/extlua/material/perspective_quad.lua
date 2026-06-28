local render = require "soluna.render"
local matext = require "soluna.material.ext"
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
bindings:view(0, state.views.storage)
bindings:sampler(0, state.default_sampler)

return matext.new {
	id = ctx.id,
	instance_size = pqmat.instance_size,
	inst_buffer = inst_buffer,
	bindings = bindings,
	uniform = state.uniform,
	sr_buffer = state.srbuffer_mem,
	sprite_bank = ctx.arg.bank_ptr,
	texture_views = state.views,
	texture_view_slot = 1,
	hooks = pqmat.hooks,
	label = "extlua-perspective-quad-pipeline",
}
