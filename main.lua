local sched = require "schedule"

sched.init(4)
local chan = sched.channel()
sched.task("hello.lua", "a", "b")
sched.task("producer.lua", chan)
sched.task("consumer.lua", chan)
sched.run()
