local render = require "soluna.render"
local pqmat = require "soluna.material.perspective_quad"

return function(register)
	register {
		name = "perspective_quad",
		create = function(ctx)
			local state = ctx.state
			state.perspective_quad_inst = render.buffer {
				type = "vertex",
				usage = "stream",
				label = "perspective-quad-instance",
				size = pqmat.instance_size * ctx.settings.draw_instance,
			}

			local perspective_quad_bindings = render.bindings()
			perspective_quad_bindings:vbuffer(0, state.perspective_quad_inst)
			perspective_quad_bindings:sampler(0, state.default_sampler)

			state.perspective_quad_bindings = perspective_quad_bindings
			state.material_perspective_quad = pqmat.new {
				inst_buffer = state.perspective_quad_inst,
				bindings = state.perspective_quad_bindings,
				uniform = state.uniform,
				sprite_bank = ctx.arg.bank_ptr,
				tmp_buffer = ctx.tmp_buffer,
			}

			return {
				reset = function()
					perspective_quad_bindings:base(0)
				end,
				submit = function(ptr, n)
					state.material_perspective_quad:submit(ptr, n)
				end,
				draw = function(ptr, n, tex)
					perspective_quad_bindings:view(1, state.views[tex + 1])
					state.material_perspective_quad:draw(ptr, n, tex)
				end,
			}
		end,
	}
end
