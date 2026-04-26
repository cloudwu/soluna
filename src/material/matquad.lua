local render = require "soluna.render"
local quadmat = require "soluna.material.quad"

return function(register)
	register {
		name = "quad",
		create = function(ctx)
			local state = ctx.state
			state.quad_inst = render.buffer {
				type = "vertex",
				usage = "stream",
				label = "quad-instance",
				size = quadmat.instance_size * ctx.settings.draw_instance,
			}

			local quad_bindings = render.bindings()
			quad_bindings:vbuffer(0, state.quad_inst)
			quad_bindings:view(0, state.views.storage)

			state.quad_bindings = quad_bindings
			state.material_quad = quadmat.new {
				inst_buffer = state.quad_inst,
				bindings = state.quad_bindings,
				uniform = state.uniform,
				sr_buffer = state.srbuffer_mem,
				tmp_buffer = ctx.tmp_buffer,
			}

			return {
				reset = function()
					quad_bindings:base(0)
				end,
				submit = function(ptr, n)
					state.material_quad:submit(ptr, n)
				end,
				draw = function(ptr, n)
					state.material_quad:draw(ptr, n)
				end,
			}
		end,
	}
end
