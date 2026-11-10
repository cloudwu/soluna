local mattarget = require "soluna.material.target"

local ctx = ...
mattarget.set_material_id(ctx.id)
ctx.state.target_material = ctx.id

local material = {}

function material.submit()
end

function material.draw()
end

return material
