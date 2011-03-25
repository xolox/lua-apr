--[[

 Unit tests for the shared memory module of the Lua/APR binding.

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
local testdata = [[
  1
  3.1
  100
  0xCAFEBABE
  0xDEADBEEF
  3.141592653589793115997963468544185161590576171875
  this line is in fact not a number :-)

  that was an empty line]]

-- Write test data to file.
local tmp_path = helpers.tmpname()
helpers.writefile(tmp_path, testdata)

-- Create shared memory segment.
local shm_path = assert(helpers.tmpname())
local shm_file = assert(apr.shm_create(shm_path, #testdata))
assert(tostring(shm_file):find '^shared memory %([0x%x]+%)$')

-- Launch child process.
local child = assert(apr.proc_create('lua'))
assert(child:cmdtype_set 'shellcmd/env')
assert(child:exec { helpers.scriptpath 'shm-child.lua', shm_path, tmp_path })
assert(child:wait(true))

-- Execute buffered I/O tests.
local test_buffer = require(((...):gsub('shm$', 'io_buffer')))
test_buffer(tmp_path, shm_file)

-- Destroy the shared memory segment.
assert(shm_file:destroy())
