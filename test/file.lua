local file = require "soluna.file"
local image = require "soluna.image"
local lfs = require "soluna.lfs"

print_r(image.info(file.load "asset/avatar.png"))
print(lfs.realpath ".")

for name in lfs.dir "." do
	print_r(name, lfs.attributes(name))
end
