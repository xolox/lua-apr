--[[

 Unit tests for the pollset module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: November 6, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

local apr = require 'apr'
local helpers = require 'apr.test.helpers'
local SERVER_PORT = math.random(10000, 40000)
local NUM_CLIENTS = 25

function main()
  local server_thread = apr.thread(server_loop, SERVER_PORT, NUM_CLIENTS)
  apr.sleep(1)
  local client_threads = {}
  for i = 1, NUM_CLIENTS do
    table.insert(client_threads, apr.thread(client_loop, SERVER_PORT, i))
  end
  local _, served_clients = assert(server_thread:join())
  for i = 1, NUM_CLIENTS do
    assert(client_threads[i]:join())
  end
  assert(served_clients == NUM_CLIENTS)
end

function server_loop(server_port, num_clients)
  local apr = require 'apr'
  -- Create the pollset object (+1 for the server socket).
  local pollset = assert(apr.pollset(num_clients + 1))
  -- Create a server socket.
  local server = assert(apr.socket_create())
  assert(server:bind('*', server_port))
  assert(server:listen(num_clients))
  -- Add the server socket to the pollset.
  assert(pollset:add(server, 'input'))
  -- Loop to handle connections.
  local counter = 0
  while counter < num_clients do
    local readable = assert(pollset:poll(-1))
    for _, socket in ipairs(readable) do
      if socket == server then
        local client = assert(server:accept())
        assert(pollset:add(client, 'input'))
      else
        assert('first request line' == assert(socket:read()))
        assert(socket:write 'first response line\n')
        assert('second request line' == assert(socket:read()))
        assert(socket:write 'second response line\n')
        assert(pollset:remove(socket))
        assert(socket:close())
        counter = counter + 1
      end
    end
  end
  -- Remove the server socket from the pollset.
  assert(pollset:remove(server))
  assert(server:close())
  -- Destroy the pollset.
  assert(pollset:destroy())
  return counter
end

function client_loop(server_port, client_nr)
  local apr = require 'apr'
  local socket = assert(apr.socket_create())
  assert(socket:connect('127.0.0.1', server_port))
  assert(socket:write 'first request line\n')
  assert('first response line' == assert(socket:read()))
  assert(socket:write 'second request line\n')
  assert('second response line' == assert(socket:read()))
  assert(socket:close())
end

main()

-- vim: ts=2 sw=2 et tw=79 fen fdm=marker
