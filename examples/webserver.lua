--[[

  Example: Single threaded webserver

  Author: Peter Odding <peter@peterodding.com>
  Last Change: March 6, 2011
  Homepage: http://peterodding.com/code/lua/apr/
  License: MIT

  The following script implements a minimalistic webserver on top of Lua/APR.
  It should work out of the box on Windows and UNIX, although you might get a
  prompt from your firewall. Once the server is running you can open
  <http://localhost:8080> in your web browser to see the server in action.
  Because the server is single threaded I was curious how bad it would perform,
  so I tested it with [ApacheBench](http://en.wikipedia.org/wiki/ApacheBench):

      $ lua examples/webserver.lua &
      $ ab -qt5 http://localhost:8080/ | grep 'Requests per second\|Transfer rate'
      Requests per second:    3672.19 [#/sec] (mean)
      Transfer rate:          2201.88 [Kbytes/sec] received

  That's not too bad for 40 lines of code! Here is the script:

]]

local port_number = tonumber(arg[1]) or 8080
local bind_address = arg[2] or '*'

-- Load the Lua/APR binding.
local apr = require 'apr'

-- Initialize the server socket.
local server = assert(apr.socket_create())
assert(server:bind(bind_address, port_number))
assert(server:listen(1))
print("Running webserver on http://" .. bind_address .. ":" .. port_number .. " ..")

-- Wait for clients to serve.
local visitor = 1
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
    <p><em>You are visitor number %010i.</em></p>
    <p>The headers provided by your web browser:</p>
    <dl>%s</dl>
  </body>
</html>
]]
while true do
  local status, message = pcall(function()
    local client = assert(server:accept())
    -- Read the HTTP request so that the client can receive data.
    local request = assert(client:read(), "Failed to receive request from client!")
    local method, location, protocol = assert(request:match '^(%w+)%s+(%S+)%s+(%S+)')
    local headers = {}
    for line in client:lines() do
      local name, value = line:match '^(%S+):%s+(.-)$'
      if not name then break end
      table.insert(headers, '<dt>' .. name .. ':</dt><dd>' .. value .. '</dd>')
    end
    -- Generate the HTTP response.
    table.sort(headers)
    content = template:format(visitor, table.concat(headers))
    client:write(protocol, ' 200 OK\r\n',
                 'Content-Type: text/html\r\n',
                 'Content-Length: ' .. #content .. '\r\n',
                 'Connection: close\r\n',
                 '\r\n',
                 content)
    assert(client:close())
    visitor = visitor + 1
  end)
  if not status then
    print('Error while serving request:', message)
  end
end

-- vim: ts=2 sw=2 et
