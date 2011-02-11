--[[

 Driver script for the unit tests of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: February 11, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

-- TODO Cleanup and extend the tests for `filepath.c'.
-- TODO Add tests for file:seek() (tricky to get right!)
-- TODO Add tests for apr.glob()!

local apr = require 'apr'
local helpers = require 'apr.test.helpers'
local modules = {
  'base64', 'crypt', 'date', 'dbd', 'dbm', 'env', 'filepath', 'fnmatch',
  'io_dir', 'io_file', 'io_net', 'misc', 'proc', 'shm', 'str', 'thread',
  'time', 'uri', 'user', 'uuid', 'xlate', 'xml'
}
for _, testname in ipairs(modules) do
  helpers.message("Running %s tests ..", testname)
  local modname = string.format('%s.%s', ..., testname)
  package.loaded[modname] = nil
  require(modname)
  helpers.message("Running %s tests: OK\n", testname)
end
helpers.message("Done!\n")
os.exit()

-- vim: ts=2 sw=2 et
