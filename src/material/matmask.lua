local render = require "soluna.render"
local maskmat = require "soluna.material.mask"

local ctx = ...
maskmat.set_material_id(ctx.id)

local mask_inst = render.buffer {
	type = "vertex",
	label = "mask-instance",
	size = maskmat.instance_size * ctx.settings.draw_instance,
}

local mask_bindings = render.bindings()
mask_bindings:vbuffer(0, mask_inst)
mask_bindings:view(0, ctx.views.storage)
mask_bindings:sampler(0, ctx.default_sampler)

local mask_bindings = mask_bindings

local material = {
	cobj = maskmat.new {
		inst_buffer = mask_inst,
		bindings = mask_bindings,
		sr_buffer = ctx.sr_buffer,
		sprite_bank = ctx.bank_ptr,
		transient_buffer = ctx.transient_buffer,
	}
}

return material
