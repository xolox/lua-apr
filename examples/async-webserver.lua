--[[

  Example: Asynchronous webserver

  Author: Peter Odding <peter@peterodding.com>
  Last Change: November 6, 2011
  Homepage: http://peterodding.com/code/lua/apr/
  License: MIT

  We can do even better than the performance of the multi threaded webserver by
  using the [APR pollset module] [pollset_module]. The following webserver uses
  asynchronous network communication to process requests from multiple clients
  'in parallel' without using multiple threads or processes. Here is a
  benchmark of the asynchronous code listed below (again using [ApacheBench]
  [ab] with the `-c` argument):

      $ CONCURRENCY=4
      $ POLLSET_SIZE=10
      $ lua examples/async-webserver.lua $POLLSET_SIZE 8080 cheat &
      $ ab -qt5 -c$CONCURRENCY http://localhost:8080/ | grep 'Requests per second\|Transfer rate'
      Requests per second:    11312.64 [#/sec] (mean)
      Transfer rate:          6219.74 [Kbytes/sec] received

  The [single threaded webserver] [simple_server] handled 3670 requests per
  second, the [multi threaded webserver] [threaded_server] handled 9210
  requests per second and the asynchronous webserver below can handle 11310
  requests per second. Actually in the above benchmark I may have cheated a bit
  (depending on whether your goal is correct usage or performance). When I
  started writing this asynchronous server example I didn't bother to add
  writable sockets to the pollset, instead I handled the request and response
  once the client socket was reported as readable by the pollset. Later on I
  changed the code to handle writing using the pollset and I noticed that the
  performance dropped. This is probably because the example is so contrived.
  Anyway here's the performance without cheating:

      $ lua examples/async-webserver.lua $POLLSET_SIZE 8080 &
      $ ab -qt5 -c$CONCURRENCY http://localhost:8888/ | grep 'Requests per second\|Transfer rate'
      Requests per second:    9867.17 [#/sec] (mean)
      Transfer rate:          5425.03 [Kbytes/sec] received

  Now follows the implementation of the asynchronous webserver example:

  [pollset_module]: #pollset
  [ab]: http://en.wikipedia.org/wiki/ApacheBench
  [simple_server]: #example_single_threaded_webserver
  [threaded_server]: #example_multi_threaded_webserver

]]

local pollset_size = tonumber(arg[1]) or 10
local port_number = tonumber(arg[2]) or 8080
local cheat = arg[3] == 'cheat' -- cheat to make it faster?

local template = [[
<html>
  <head>
    <title>Hello from Lua/APR!</title>
    <style type="text/css">
      body { font-family: sans-serif; }
      dt { font-weight: bold; }
      dd { font-family: monospace; margin: -1.4em 0 0 14em; }
    </style>
  </head>
  <body>
    <h1>Hello from Lua/APR!</h1>
    <p>The headers provided by your web browser:</p>
    <dl>%s</dl>
  </body>
</html>
]]

-- Load the Lua/APR binding.
local apr = require 'apr'

-- Initialize the server socket.
local server = assert(apr.socket_create())
assert(server:bind('*', port_number))
assert(server:listen(pollset_size))

-- Create the pollset.
local pollset = assert(apr.pollset(pollset_size))
assert(pollset:add(server, 'input'))

-- Use a table indexed with socket objects to keep track of "sessions".
local sessions = setmetatable({}, {__mode='k'})

-- Enter the I/O loop.
print("Running webserver on http://localhost:" .. port_number .. " ..")
while true do
  local readable, writable = assert(pollset:poll(-1))
  -- Process requests.
  for _, socket in ipairs(readable) do
    if socket == server then
      local client = assert(server:accept())
      assert(pollset:add(client, 'input'))
    else
      local request = assert(socket:read(), "Failed to receive request from client!")
      local method, location, protocol = assert(request:match '^(%w+)%s+(%S+)%s+(%S+)')
      local headers = {}
      for line in socket:lines() do
        local name, value = line:match '^(%S+):%s+(.-)$'
        if not name then
          break
        end
        table.insert(headers, '<dt>' .. name .. ':</dt><dd>' .. value .. '</dd>')
      end
      table.sort(headers)
      local content = template:format(table.concat(headers))
      assert(socket:write(
        protocol, ' 200 OK\r\n',
        'Content-Type: text/html\r\n',
        'Content-Length: ', #content, '\r\n',
        'Connection: close\r\n',
        '\r\n'))
      if cheat then
        assert(socket:write(content))
        assert(pollset:remove(socket))
        assert(socket:close())
      else
        sessions[socket] = content
        assert(pollset:remove(socket))
        assert(pollset:add(socket, 'output'))
      end
    end
  end
  if not cheat then
    -- Process responses.
    for _, socket in ipairs(writable) do
      assert(socket:write(sessions[socket]))
      -- I don't like that when I switch the order of these
      -- calls, it breaks... Seems like a fairly big gotcha.
      assert(pollset:remove(socket))
      assert(socket:close())
    end
  end
end

-- vim: ts=2 sw=2 et
