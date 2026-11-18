local clipmat = require "soluna.material.clip"

local ctx = ...
clipmat.set_material_id(ctx.id)

local material = {
	cobj = clipmat.new { uniform = ctx.uniform }
}

function material.reset()
end

return material
