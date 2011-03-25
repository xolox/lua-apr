local status, apr = pcall(require, 'apr')
if not status then
  pcall(require, 'luarocks.require')
  apr = require 'apr'
end
local namedpipe = assert(table.remove(arg, 1))
local namedmsg = assert(table.concat(arg, ' '))
local handle = assert(apr.file_open(namedpipe, 'w'))
assert(handle:write(namedmsg))
assert(handle:close())
