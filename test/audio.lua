local soluna = require "soluna"

soluna.load_sounds "asset/sounds.dl"
soluna.set_window_title "Soluna sound sample"

local callback = {}

soluna.play_sound "bloop"

function callback.frame(count)
end

return callback

