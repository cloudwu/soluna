local render = require "soluna.render"
local textmat = require "soluna.material.text"

return function(register)
	register {
		name = "text",
		create = function(ctx)
			local state = ctx.state
			local setting = ctx.settings
			local text_bindings
			local text_sampler_desc = setting.text_sampler
			if text_sampler_desc then
				text_sampler_desc.label = text_sampler_desc.label or "text-sampler"
				state.text_sampler = render.sampler(text_sampler_desc)
				state.text_inst = render.buffer {
					type = "vertex",
					usage = "stream",
					label = "text-instance",
					size = textmat.instance_size * setting.draw_instance,
				}
				text_bindings = render.bindings()
				text_bindings:vbuffer(0, state.text_inst)
				text_bindings:view(0, state.views.storage)
				text_bindings:sampler(0, state.text_sampler)
			else
				state.text_inst = state.inst
				text_bindings = state.bindings
			end
			state.text_bindings = text_bindings
			state.material_text = textmat.normal {
				inst_buffer = state.text_inst,
				bindings = state.text_bindings,
				uniform = state.uniform,
				sr_buffer = state.srbuffer_mem,
				font_manager = ctx.font.cobj,
				tmp_buffer = ctx.tmp_buffer,
			}

			return {
				reset = function()
					text_bindings:base(0)
				end,
				submit = function(ptr, n)
					state.material_text:submit(ptr, n)
				end,
				draw = function(ptr, n)
					text_bindings:view(1, state.views.font)
					state.material_text:draw(ptr, n)
				end,
			}
		end,
	}
end
