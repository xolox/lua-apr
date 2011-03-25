--[[

 Unit tests for the network I/O handling module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: March 27, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

local status, apr = pcall(require, 'apr')
if not status then
  pcall(require, 'luarocks.require')
  apr = require 'apr'
end
local helpers = require 'apr.test.helpers'

-- Test apr.hostname_get(), apr.host_to_addr() and apr.addr_to_host().
local hostname = assert(apr.hostname_get())
local address = assert(apr.host_to_addr(hostname))
assert(apr.addr_to_host(address))

-- Test socket:bind(), socket:listen() and socket:accept().
local server = assert(apr.proc_create 'lua')
local port = 12345
local signalfile = helpers.tmpname()
local scriptfile = helpers.scriptpath 'io_net-server.lua'
assert(server:cmdtype_set 'shellcmd/env')
assert(server:exec { scriptfile, port, signalfile })
assert(helpers.wait_for(signalfile, 15), "Failed to initialize " .. scriptfile)
local client = assert(apr.socket_create())
assert(client:connect('localhost', port))
for _, msg in ipairs { 'First line', 'Second line', 'Third line' } do
  assert(client:write(msg, '\n'))
  assert(client:read() == msg:upper())
end

-- Test tostring(socket).
assert(tostring(client):find '^socket %([x%x]+%)$')
assert(client:close())
assert(tostring(client):find '^socket %(closed%)$')
