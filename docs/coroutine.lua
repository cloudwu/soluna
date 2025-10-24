---@meta soluna.coroutine

---
--- Soluna coroutine module
---

---@class soluna.coroutine
local coroutine = {}

---
--- Creates a new coroutine
---
--- Creates a coroutine compatible with ltask framework.
--- The coroutine is tracked for ltask yield/resume handling.
---
---@param f function Coroutine function
---@return thread co Coroutine thread object
function coroutine.create(f) end

---
--- Resumes a coroutine
---
--- Resumes execution of a coroutine. Handles ltask framework yielding automatically.
--- Returns true and results on success, or false and error message on failure.
---
---@param co thread Coroutine to resume
---@param ... any Arguments to pass to coroutine
---@return boolean success True if resumed successfully
---@return any ... Return values from coroutine or error message
function coroutine.resume(co, ...) end

---
--- Yields from current coroutine
---
--- Suspends execution of current coroutine and returns control to caller.
--- Compatible with ltask framework.
---
---@param ... any Values to return to resume caller
---@return any ... Values passed from resume
function coroutine.yield(...) end

return coroutine
