local sched = require "schedule"
local c = ...
assert(type(c) == "number")
local taskid = sched.taskid()

for i=1,10 do
	sched.write(c, i)
	print(string.format("task %d send %d", taskid, i))
	coroutine.yield()
end
