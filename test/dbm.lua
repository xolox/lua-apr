--[[

 Unit tests for the DBM module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: June 30, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

local status, apr = pcall(require, 'apr')
if not status then
  pcall(require, 'luarocks.require')
  apr = require 'apr'
end
local helpers = require 'apr.test.helpers'
local dbmfile = helpers.tmpname()
local dbm = assert(apr.dbm_open(dbmfile, 'n'))
local dbmkey, dbmvalue = 'the key', 'the value'
assert(not dbm:firstkey()) -- nothing there yet
assert(dbm:store(dbmkey, dbmvalue))
local function checkdbm()
  assert(dbm:exists(dbmkey))
  assert(dbm:fetch(dbmkey) == dbmvalue)
  assert(dbm:firstkey() == dbmkey)
  assert(not dbm:nextkey(dbmkey)) -- only 1 record exists
end
checkdbm()
assert(dbm:close())
local file1, file2 = assert(apr.dbm_getnames(dbmfile))
assert(apr.stat(file1, 'type') == 'file')
assert(not file2 or apr.stat(file2, 'type') == 'file')
dbm = assert(apr.dbm_open(dbmfile, 'w'))
checkdbm()
assert(dbm:delete(dbmkey))
assert(not dbm:fetch(dbmkey))
assert(not dbm:firstkey())

-- Test tostring(dbm).
assert(tostring(dbm):find '^dbm %([x%x]+%)$')
assert(dbm:close())
assert(tostring(dbm):find '^dbm %(closed%)$')

-- Cleanup.
apr.file_remove(file1)
if file2 then
  apr.file_remove(file2)
end
