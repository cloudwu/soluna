---@meta soluna.coroutine

---ltask 兼容 coroutine 模块
---ltask-compatible coroutine module.
---@class soluna.coroutine
local coroutine = {}

---创建受 ltask 跟踪的 coroutine
---Creates a coroutine tracked by the ltask bridge.
---@param f function coroutine 函数 / Coroutine function
---@return thread co coroutine 线程 / Coroutine thread
function coroutine.create(f)
end

---恢复 coroutine
---Resumes a tracked coroutine.
---@param co thread coroutine 线程 / Coroutine thread
---@param ... any 传入参数 / Arguments
---@return boolean ok 是否成功 / Whether resume succeeded
---@return any ... 返回值或错误 / Return values or error
function coroutine.resume(co, ...)
end

---挂起当前 coroutine
---Yields from the current coroutine.
---@param ... any 返回给 resume 的值 / Values returned to resume
---@return any ... 下次 resume 传入的值 / Values passed by the next resume
function coroutine.yield(...)
end

return coroutine
