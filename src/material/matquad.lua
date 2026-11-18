local render = require "soluna.render"
local quadmat = require "soluna.material.quad"

local ctx = ...

quadmat.set_material_id(ctx.id)

local quad_inst = render.buffer {
	type = "vertex",
	label = "quad-instance",
	size = quadmat.instance_size * ctx.settings.draw_instance,
}

local quad_bindings = render.bindings()
quad_bindings:vbuffer(0, quad_inst)
quad_bindings:view(0, ctx.views.storage)

local material = {
	cobj = quadmat.new {
		inst_buffer = quad_inst,
		bindings = quad_bindings,
		sr_buffer = ctx.sr_buffer,
		transient_buffer = ctx.transient_buffer,
	}
}

return material
