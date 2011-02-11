--[[

 Unit tests for the shared memory module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: February 11, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

local apr = require 'apr'
local helpers = require 'apr.test.helpers'

-- Write test data to temporary file.
local tmp_path = assert(helpers.tmpname())
local tmp_file = assert(io.open(tmp_path, 'w'))
assert(tmp_file:write [[
  1
  3.1
  100
  0xCAFEBABE
  0xDEADBEEF
  3.141592653589793115997963468544185161590576171875
  this line is in fact not a number :-)

  that was an empty line]])
local shm_size = tmp_file:seek()
assert(tmp_file:close())
tmp_file = assert(io.open(tmp_path))

-- Create shared memory segment.
local shm_path = assert(helpers.tmpname())
local shm_file = assert(apr.shm_create(shm_path, shm_size))
assert(tostring(shm_file):find '^shared memory %([0x%x]+%)$')

-- Launch child process.
local child = assert(apr.proc_create('lua'))
assert(child:cmdtype_set 'shellcmd')
assert(child:exec { helpers.scriptpath 'shm-child.lua', shm_path, tmp_path })
assert(child:wait(true))

-- Compare results of several read formats between regular files / shared memory objects.
for _, format in ipairs { 1, 2, 3, 4, 5, 100, 1000, '*n', '*l', '*a' } do
  while true do
    local tmp_val = tmp_file:read(format)
    local shm_val = shm_file:read(format)
    assert(tmp_val == shm_val)
    if tmp_val == nil or (format == '*a' and tmp_val == '') then
      break
    end
  end
  assert(tmp_file:seek('set', 0))
  assert(shm_file:seek('set', 0))
end

assert(shm_file:destroy())
