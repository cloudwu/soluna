local render = require "soluna.render"
local textmat = require "soluna.material.text"

local ctx = ...
textmat.set_material_id(ctx.id)

local text_bindings = render.bindings()
local text_inst = render.buffer {
	type = "vertex",
	label = "text-instance",
	size = textmat.instance_size * ctx.settings.draw_instance,
}
text_bindings:vbuffer(0, text_inst)
text_bindings:view(0, ctx.views.storage)
local text_sampler
if ctx.settings.text_sampler then
	text_sampler = render.sampler(ctx.settings.text_sampler or "text-sampler")
	text_bindings:sampler(0, text_sampler)
else
	text_bindings:sampler(0, ctx.default_sampler)
end

local material = {
	name = "text",
	sampler = text_sampler,
	cobj = textmat.normal {
		inst_buffer = text_inst,
		bindings = text_bindings,
		sr_buffer = ctx.sr_buffer,
		font_manager = ctx.font.cobj,
		transient_buffer = ctx.transient_buffer,
	},
}

return material
