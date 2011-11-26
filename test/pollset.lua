--[[

 Unit tests for the pollset module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: November 20, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

local apr = require 'apr'
local SERVER_PORT = math.random(10000, 40000)
local NUM_CLIENTS = 25
local main, server_loop, client_loop

function main() -- {{{1
  local server_thread = assert(apr.thread(server_loop))
  apr.sleep(0.25)
  local client_threads = {}
  for i = 1, NUM_CLIENTS do
    table.insert(client_threads, assert(apr.thread(client_loop)))
  end
  local _, served_clients = assert(server_thread:join())
  for i = 1, NUM_CLIENTS do
    assert(client_threads[i]:join())
  end
  assert(served_clients == NUM_CLIENTS)
end

function server_loop() -- {{{1
  local apr = require 'apr'
  -- Create the pollset object (+1 for the server socket).
  local pollset = assert(apr.pollset(NUM_CLIENTS + 1))
  -- Create a server socket.
  local server = assert(apr.socket_create())
  assert(server:bind('*', SERVER_PORT))
  assert(server:listen(NUM_CLIENTS))
  -- Add the server socket to the pollset.
  assert(pollset:add(server, 'input'))
  -- Loop to handle connections.
  local counter = 0
  while counter < NUM_CLIENTS do
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

function client_loop() -- {{{1
  local apr = require 'apr'
  local socket = assert(apr.socket_create())
  assert(socket:connect('127.0.0.1', SERVER_PORT))
  assert(socket:write 'first request line\n')
  assert('first response line' == assert(socket:read()))
  assert(socket:write 'second request line\n')
  assert('second response line' == assert(socket:read()))
  assert(socket:close())
end

-- }}}

main()

-- vim: ts=2 sw=2 et tw=79 fen fdm=marker
