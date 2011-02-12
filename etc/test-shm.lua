local apr = require 'apr'
local shm_file = assert(apr.shm_attach(arg[1]))
local tmp_file = assert(io.open(arg[2]))
assert(shm_file:write(tmp_file:read('*a')))

-- You don't actually have to call this, I'm
-- just testing that it works as advertised :-)
assert(shm_file:detach())

-- Check that removing works.
assert(apr.shm_remove(arg[1]))
assert(not apr.shm_attach(arg[1]))
