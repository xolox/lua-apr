local apr = assert(require 'apr')
local server = assert(apr.socket_create())
assert(server:bind('*', arg[1]))
assert(server:listen(1))
io.write(' (server listening on port ', arg[1], ')\n')
local client = assert(server:accept())
for line in client:lines() do
  assert(client:write(line:upper(), '\n'))
end
assert(client:close())
