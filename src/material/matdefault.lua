local render = require "soluna.render"
local defmat = require "soluna.material.default"

local ctx = ...
local inst_buffer = render.buffer {
	type = "vertex",
	label = "texquad-instance",
	size = defmat.instance_size * ctx.settings.draw_instance,
}
local bindings = render.bindings()
bindings:vbuffer(0, inst_buffer)
bindings:view(0, ctx.views.storage)
bindings:sampler(0, ctx.default_sampler)

local material = {
	cobj = defmat.new {
		inst_buffer = inst_buffer,
		bindings = bindings,
		sr_buffer = ctx.sr_buffer,
		sprite_bank = ctx.bank_ptr,
		transient_buffer = ctx.transient_buffer,
	}
}

return material
