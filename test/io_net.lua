--[[

 Unit tests for the network I/O handling module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: November 26, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

local status, apr = pcall(require, 'apr')
if not status then
  pcall(require, 'luarocks.require')
  apr = require 'apr'
end
local helpers = require 'apr.test.helpers'

-- Test apr.hostname_get(), apr.host_to_addr() and apr.addr_to_host(). {{{1
local hostname = assert(apr.hostname_get())
local address = assert(apr.host_to_addr(hostname))
-- https://github.com/xolox/lua-apr/issues/10
helpers.soft_assert(helpers.pack(apr.addr_to_host(address)),
    'This test can apparently fail on Mac OS X so I made it a soft assertion')

-- Test socket:bind(), socket:listen() and socket:accept(). {{{1
local server = assert(apr.proc_create 'lua')
local port = math.random(10000, 50000)
local signalfile = helpers.tmpname()
local scriptfile = helpers.scriptpath 'io_net-server.lua'
assert(server:cmdtype_set 'shellcmd/env')
assert(server:exec { scriptfile, port, signalfile })
assert(helpers.wait_for(signalfile, 15), "Failed to initialize " .. scriptfile)
local client = assert(apr.socket_create())
assert(client:connect('localhost', port))
for _, msg in ipairs { 'First line', 'Second line', 'Third line' } do
  assert(client:write(msg, '\n'))
  assert(msg:upper() == assert(client:read()))
end

-- Test socket:fd_get() and socket:fd_set(). {{{1
local fd = assert(client:fd_get())
local thread = assert(apr.thread(function()
  -- Load the Lua/APR binding.
  local apr = require 'apr'
  -- Convert file descriptor to socket.
  local client = assert(apr.socket_create())
  assert(client:fd_set(fd))
  local msg = 'So does it expose file descriptors?'
  assert(client:write(msg, '\n'))
  assert(msg:upper() == assert(client:read()))
  -- Test tostring(socket).
  assert(tostring(client):find '^socket %([x%x]+%)$')
  assert(client:close())
  assert(tostring(client):find '^socket %(closed%)$')
end))
assert(thread:join())

-- Test UDP server socket using socket:recvfrom() . {{{1

local udp_port = math.random(10000, 50000)
local udp_socket = assert(apr.socket_create 'udp')
assert(udp_socket:bind('*', udp_port))

local server = assert(apr.thread(function()
  local client, data = assert(udp_socket:recvfrom())
  assert(client.address == '127.0.0.1')
  assert(client.port >= 1024)
  assert(data == 'booyah!')
end))

local client = assert(apr.thread(function()
  local apr = require 'apr'
  local socket = assert(apr.socket_create 'udp')
  assert(socket:connect('127.0.0.1', udp_port))
  assert(socket:write 'booyah!')
  assert(socket:close())
end))

assert(server:join())
assert(client:join())
