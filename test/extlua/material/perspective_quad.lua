local render = require "soluna.render"
local matext = require "soluna.material.ext"
local pqmat = require "ext.material.perspective_quad"

local ctx = ...

pqmat.set_material_id(ctx.id)

local inst_buffer = render.buffer {
	type = "vertex",
	label = "extlua-perspective-quad-instance",
	size = pqmat.instance_size * ctx.settings.draw_instance,
}

local bindings = render.bindings()
bindings:vbuffer(0, inst_buffer)
bindings:view(0, ctx.views.storage)
bindings:sampler(0, ctx.default_sampler)

return {
	cobj = matext.new {
		id = ctx.id,
		instance_size = pqmat.instance_size,
		inst_buffer = inst_buffer,
		transient_buffer = ctx.transient_buffer,
		bindings = bindings,
		uniform_slot = 0,
		sr_buffer = ctx.sr_buffer,
		sprite_bank = ctx.bank_ptr,
		texture_view_slot = 1,
		hooks = pqmat.hooks,
		label = "extlua-perspective-quad-pipeline",
	}
}
