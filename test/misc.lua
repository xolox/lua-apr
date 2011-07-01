--[[

 Unit tests for the miscellaneous routines of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: July 1, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

local status, apr = pcall(require, 'apr')
if not status then
  pcall(require, 'luarocks.require')
  apr = require 'apr'
end
local helpers = require 'apr.test.helpers'

-- Test apr.type()
local selfpath = helpers.filedefined()
assert(apr.type(apr.file_open(selfpath)) == 'file')
assert(apr.type(apr.socket_create()) == 'socket')
assert(apr.type(apr.proc_create 'test') == 'process')
assert(apr.type(apr.dir_open '.') == 'directory')

-- Test apr.version_get()
local v = apr.version_get()
assert(v.apr:find '^%d+%.%d+%.%d+$')
assert(v.aprutil == nil or v.aprutil:find '^%d+%.%d+%.%d+$')
assert(v.apreq == nil or v.apreq:find '^%d+%.%d+%.%d+$')

-- Test status_to_name() (indirectly).
assert(select(3, apr.stat("I assume this won't exist")) == 'ENOENT')

-- Test apr.os_default/locale_encoding()
local default = apr.os_default_encoding()
local locale = apr.os_locale_encoding()
assert(type(default) == 'string' and default:find '%S')
assert(type(locale) == 'string' and locale:find '%S')
