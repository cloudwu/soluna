local initsetting = require "soluna.initsetting"

global assert

local S = {}

local setting

function S.init(args)
	assert(setting == nil)
	setting = initsetting.init(args, true)
end

function S.get()
	return setting
end

return S
