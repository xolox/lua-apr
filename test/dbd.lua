--[[

 Unit tests for the relational database module of the Lua/APR binding.

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

-- XXX This hack is needed to make the tests pass on Ubuntu 10.04 and probably
-- also other versions of Ubuntu and Debian? The Lua/APR documentation for the
-- DBD module contains some notes about this, here's a direct link:
-- http://peterodding.com/code/lua/apr/docs/#debugging_dso_load_failed_errors
local libs = apr.filepath_list_split(apr.env_get 'LD_PRELOAD' or '')
for _, libname in ipairs { '/usr/lib/libapr-1.so.0', '/usr/lib/libaprutil-1.so.0' } do
  if apr.stat(libname, 'type') == 'file' then table.insert(libs, libname) end
end
assert(apr.env_set('LD_PRELOAD', apr.filepath_list_merge(libs)))

local child = assert(apr.proc_create 'lua')
assert(child:cmdtype_set 'shellcmd/env')
assert(child:exec { helpers.scriptpath 'dbd-child.lua' })
local dead, reason, code = assert(child:wait(true))
return reason == 'exit' and code == 0
