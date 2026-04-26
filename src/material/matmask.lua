local render = require "soluna.render"
local maskmat = require "soluna.material.mask"

return function(register)
	register {
		name = "mask",
		create = function(ctx)
			local state = ctx.state
			state.mask_inst = render.buffer {
				type = "vertex",
				usage = "stream",
				label = "mask-instance",
				size = maskmat.instance_size * ctx.settings.draw_instance,
			}

			local mask_bindings = render.bindings()
			mask_bindings:vbuffer(0, state.mask_inst)
			mask_bindings:view(0, state.views.storage)
			mask_bindings:sampler(0, state.default_sampler)

			state.mask_bindings = mask_bindings
			state.material_mask = maskmat.new {
				inst_buffer = state.mask_inst,
				bindings = state.mask_bindings,
				uniform = state.uniform,
				sr_buffer = state.srbuffer_mem,
				sprite_bank = ctx.arg.bank_ptr,
				tmp_buffer = ctx.tmp_buffer,
			}

			return {
				reset = function()
					mask_bindings:base(0)
				end,
				submit = function(ptr, n)
					state.material_mask:submit(ptr, n)
				end,
				draw = function(ptr, n, tex)
					mask_bindings:view(1, state.views[tex + 1])
					state.material_mask:draw(ptr, n, tex)
				end,
			}
		end,
	}
end
