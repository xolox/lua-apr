--[[

 Unit tests for the relational database module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: February 11, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

local apr = require 'apr'
local helpers = require 'apr.test.helpers'

-- See the DBD module documentation for why this hack is here.
local libs = { '/usr/lib/libapr-1.so', '/usr/lib/libaprutil-1.so' }
if apr.env_get 'LD_PRELOAD' == nil
    and apr.stat(libs[1], 'type') == 'file'
    and apr.stat(libs[2], 'type') == 'file' then
  assert(apr.env_set('LD_PRELOAD', apr.filepath_list_merge(libs)))
end

local child = assert(apr.proc_create 'lua')
assert(child:cmdtype_set 'shellcmd/env')
assert(child:exec { helpers.scriptpath 'dbd-child.lua' })
local dead, reason, code = assert(child:wait(true))
return reason == 'exit' and code == 0
