--[[

 Driver script for the unit tests of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: February 19, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

-- TODO Cleanup and extend the tests for `filepath.c'.
-- TODO Add tests for file:seek() (tricky to get right!)
-- TODO Add tests for apr.glob()!

local apr = require 'apr'
local helpers = require 'apr.test.helpers'

-- Names of modules for which tests have been written (the individual lines
-- enable automatic rebasing between git feature branches and master branch).
local modules = {
  'base64',
  'crypt',
  'date',
  'dbd',
  'dbm',
  'env',
  'filepath',
  'fnmatch',
  'io_dir',
  'io_file',
  'io_net',
  'misc',
  'proc',
  'shm',
  'str',
  'thread',
  'thread_queue',
  'time',
  'uri',
  'user',
  'uuid',
  'xlate',
  'xml'
}

for _, testname in ipairs(modules) do
  helpers.message("Running %s tests ..", testname)
  local modname = string.format('%s.%s', ..., testname)
  package.loaded[modname] = nil
  if require(modname) then
    helpers.message("Running %s tests: OK\n", testname)
  else
    helpers.message("Running %s tests: Failed!\n", testname)
  end
  package.loaded[modname] = nil
  -- Garbage collect unreferenced objects before testing the next module.
  collectgarbage 'collect'; collectgarbage 'collect'
end
helpers.message("Done!\n")

-- Exit the interpreter (started with lua -lapr.test).
os.exit()

-- vim: ts=2 sw=2 et
