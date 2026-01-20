local spritemgr = require "soluna.spritemgr"

-- texture size = 128
local bank = spritemgr.newbank(65536, 128)

bank:add(32, 16)
bank:add(64, 32)
bank:add(96, 96)
bank:add(96, 96)

local texid, n = bank:pack()
print("Pack",n,"from",texid)

for i = 1, n do
	local r = bank:altas(texid + i - 1)
	for k,v in pairs(r) do
		r[k] = { x = v >> 32, y = v & 0xffffffff }
	end
	print_r(i, r)
end

