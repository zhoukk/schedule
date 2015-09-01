local sched = require "schedule"
local c = ...
assert(type(c) == "number")
local taskid = sched.taskid()

local function recv()
    while true do
        if sched.select(c) then
            local ok, value = sched.read(c)
            if ok then
                print(string.format("task %d recv %d", taskid, value))
                return
            end
        end
        coroutine.yield()
    end
end

for i=1,10 do
    recv()
end
