--[[

 Unit tests for the URI parsing module of the Lua/APR binding.

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
local hostinfo = 'scheme://user:pass@host:80'
local pathinfo = '/path/file?query-param=value#fragment'
local input = hostinfo .. pathinfo

-- Parse a URL into a table of components.
local parsed = assert(apr.uri_parse(input))

-- Validate the parsed URL fields.
assert(parsed.scheme   == 'scheme')
assert(parsed.user     == 'user')
assert(parsed.password == 'pass')
assert(parsed.hostname == 'host')
assert(parsed.port     == '80')
assert(parsed.hostinfo == 'user:pass@host:80')
assert(parsed.path     == '/path/file')
assert(parsed.query    == 'query-param=value')
assert(parsed.fragment == 'fragment')

-- Check that complete and partial URL `unparsing' works.
assert(apr.uri_unparse(parsed) == input)
assert(apr.uri_unparse(parsed, 'hostinfo') == hostinfo)
assert(apr.uri_unparse(parsed, 'pathinfo') == pathinfo)

-- Make sure uri_port_of_scheme() works.
assert(apr.uri_port_of_scheme 'ssh' == 22)
assert(apr.uri_port_of_scheme 'http' == 80)
assert(apr.uri_port_of_scheme 'https' == 443)
