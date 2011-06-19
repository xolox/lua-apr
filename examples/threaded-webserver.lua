--[[

  Example: Multi threaded webserver

  Author: Peter Odding <peter@peterodding.com>
  Last Change: June 19, 2011
  Homepage: http://peterodding.com/code/lua/apr/
  License: MIT

  Thanks to the [multi threading] [threading_module] and [thread queue]
  [thread_queues] modules in the Apache Portable Runtime it is possible to
  improve the performance of the single threaded webserver from the previous
  example. Here is a benchmark of the multi threaded implementation listed
  below (again using [ApacheBench] [ab], but now with the `-c` argument):

      $ NUM_THREADS=4
      $ lua examples/threaded-webserver.lua $NUM_THREADS &
      $ ab -qt5 -c$NUM_THREADS http://localhost:8080/ | grep 'Requests per second\|Transfer rate'
      Requests per second:    9210.72 [#/sec] (mean)
      Transfer rate:          5594.79 [Kbytes/sec] received

  Comparing these numbers to the benchmark of the single threaded webserver we
  can see that the number of requests per second went from 3670 to 9210, more
  than doubling the throughput of the webserver on a dual core processor.

  *Note that both benchmarks were run on my Compaq Presario CQ60 laptop (which
  features an Intel Core 2 Duo T5800 processor clocked at 2 GHz and 3 GB of
  RAM) and that the Lua/APR binding was compiled without debugging symbols.*

  [threading_module]: #multi_threading
  [thread_queues]: #thread_queues
  [ab]: http://en.wikipedia.org/wiki/ApacheBench
  [thread_pool]: http://en.wikipedia.org/wiki/Thread_pool_pattern

]]

local num_threads = tonumber(arg[1]) or 2
local port_number = tonumber(arg[2]) or 8080

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
    <p><em>This web page was served by worker %i.</em></p>
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
assert(server:listen(num_threads * 2))
print("Running webserver with " .. num_threads .. " client threads on http://localhost:" .. port_number .. " ..")

-- Create the thread queue (used to pass sockets between threads).
local queue = apr.thread_queue(num_threads)

-- Define the function to execute in each child thread.
function worker(thread_id, queue, template)
  pcall(require, 'luarocks.require')
  local apr = require 'apr'
  while true do
    local client, msg, code = queue:pop()
    assert(client or code == 'EINTR', msg)
    if client then
      local status, message = pcall(function()
        local request = assert(client:read(), "Failed to receive request from client!")
        local method, location, protocol = assert(request:match '^(%w+)%s+(%S+)%s+(%S+)')
        local headers = {}
        for line in client:lines() do
          local name, value = line:match '^(%S+):%s+(.-)$'
          if not name then
            break
          end
          table.insert(headers, '<dt>' .. name .. ':</dt><dd>' .. value .. '</dd>')
        end
        table.sort(headers)
        local content = template:format(thread_id, table.concat(headers))
        client:write(protocol, ' 200 OK\r\n',
                     'Content-Type: text/html\r\n',
                     'Content-Length: ' .. #content .. '\r\n',
                     'Connection: close\r\n',
                     '\r\n',
                     content)
        assert(client:close())
      end)
      if not status then
        print('Error while serving request:', message)
      end
    end
  end
end

-- Create the child threads and keep them around in a table (so that they are
-- not garbage collected while we are still using them).
local pool = {}
for i = 1, num_threads do
  table.insert(pool, apr.thread(worker, i, queue, template))
end

-- Enter the accept() loop in the parent thread.
while true do
  local status, message = pcall(function()
    local client = assert(server:accept())
    assert(queue:push(client))
  end)
  if not status then
    print('Error while serving request:', message)
  end
end

-- vim: ts=2 sw=2 et
