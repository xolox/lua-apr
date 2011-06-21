local status, apr = pcall(require, 'apr')
if not status then
  pcall(require, 'luarocks.require')
  apr = require 'apr'
end
local server = assert(apr.socket_create())
assert(server:bind('*', arg[1]))
assert(server:listen(1))

-- Signal to the test suite that we've initialized successfully?
local handle = assert(io.open(arg[2], 'w'))
assert(handle:write 'DONE')
assert(handle:close())

local client = assert(server:accept())
for line in client:lines() do
  assert(client:write(line:upper(), '\n'))
end
assert(client:close())
