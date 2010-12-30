--[[

  Example: HTTP client

  Author: Peter Odding <peter@peterodding.com>
  Last Change: December 30, 2010
  Homepage: http://peterodding.com/code/lua/apr/
  License: MIT

  The following Lua script implements a minimal [HTTP client] [http] which can
  be used to download a given [URL] [url] on the command line (comparable to
  [wget] [wget] and [curl] [curl]):

      $ FILE=lua-5.1.4.tar.gz
      $ URL=http://www.lua.org/ftp/$FILE
      $ time curl -s $URL > $FILE
      0,01s user 0,02s system 6% cpu 0,465 total
      $ time lua examples/download.lua $URL > $FILE
      0,03s user 0,02s system 9% cpu 0,549 total

  Note that this script and Lua/APR in general are a bit handicapped in that
  they don't support [HTTPS] [https] because the Apache Portable Runtime does
  not support encrypted network communication.

  [url]: http://en.wikipedia.org/wiki/Uniform_Resource_Locator
  [wget]: http://en.wikipedia.org/wiki/wget
  [curl]: http://en.wikipedia.org/wiki/cURL
  [https]: http://en.wikipedia.org/wiki/HTTPS

]]

local apr = require 'apr'

-- Report errors without stack traces.
local function assert(...)
  local status, message = ...
  if not status then
    io.stderr:write('Error: ', message or '(no message)', '\n')
    os.exit(1)
  end
  return ...
end

local function getpage(url)
  local components = assert(apr.uri_parse(url))
  assert(components.scheme == 'http', "invalid protocol!")
  local port = assert(components.port or apr.uri_port_of_scheme(components.scheme))
  local socket = assert(apr.socket_create())
  assert(socket:connect(components.hostname, port))
  local pathinfo = assert(apr.uri_unparse(components, 'pathinfo'))
  assert(socket:write('GET ', pathinfo, ' HTTP/1.0\r\n',
                      'Host: ', components.hostname, '\r\n',
                      '\r\n'))
  local statusline = assert(socket:read(), 'HTTP response missing status line!')
  local protocol, statuscode, reason = assert(statusline:match '^(%S+)%s+(%S+)%s+(.-)$')
  local redirect = statuscode:find '^30[123]$'
  for line in socket:lines() do
    local name, value = line:match '^(%S+):%s+(.-)\r?$'
    if name and value then
      if redirect and name:lower() == 'location' then
        io.stderr:write("Following redirect to ", value, " ..\n")
        return getpage(value)
      end
    else
      return (assert(socket:read '*a', 'HTTP response missing body?!'))
    end
  end
  if statuscode ~= '200' then error(reason) end
end

local usage = "Please provide a URL to download as argument"
io.write(getpage(assert(arg and arg[1], usage)))

-- vim: ts=2 sw=2 et
