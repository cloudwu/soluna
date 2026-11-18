local clipmat = require "soluna.material.clip"

local ctx = ...
clipmat.set_material_id(ctx.id)

local material = {
	cobj = clipmat.new {}
}

function material.reset()
end

return material
