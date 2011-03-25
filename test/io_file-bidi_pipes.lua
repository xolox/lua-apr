local status, apr = pcall(require, 'apr')
if not status then
  pcall(require, 'luarocks.require')
  apr = require 'apr'
end
local stdin = assert(apr.pipe_open_stdin())
local stdout = assert(apr.pipe_open_stdout())
local stderr = assert(apr.pipe_open_stderr())
while true do
  local input = stdin:read()
  if not input then break end
  stdout:write(input:lower(), '\n')
  stderr:write(input:upper(), '\n')
end
