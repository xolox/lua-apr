--[[

 Unit tests for the multi threading module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: November 20, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

 This script is executed as a child process by thread.lua.

--]]

local status, apr = pcall(require, 'apr')
if not status then
  pcall(require, 'luarocks.require')
  apr = require 'apr'
end
local helpers = require 'apr.test.helpers'

-- Check that yield() exists, can be called and does mostly nothing :-)
assert(select('#', apr.thread_yield()) == 0)

-- Test thread creation.
local threadfile = helpers.tmpname()
local thread = assert(apr.thread(function()
  local handle = assert(io.open(threadfile, 'w'))
  assert(handle:write 'hello world!')
  assert(handle:close())
end))
assert(thread:join())

-- Check that the file was actually created inside the thread.
assert(helpers.readfile(threadfile) == 'hello world!')

-- Test that package.config, package.path and package.cpath are preserved.
local config, path, cpath = package.config, package.path, package.cpath
local thread = assert(apr.thread(function()
  assert(package.config == config, "apr.thread() failed to preserve package.config")
  assert(package.path == path, "apr.thread() failed to preserve package.path")
  assert(package.cpath == cpath, "apr.thread() failed to preserve package.cpath")
end))
assert(thread:join())

-- Test module loading and multiple return values.
local thread = assert(apr.thread(function()
  local status, apr = pcall(require, 'apr')
  if not status then
    pcall(require, 'luarocks.require')
    apr = require 'apr'
  end
  return apr.version_get().apr
end))
helpers.checktuple({ true, apr.version_get().apr }, assert(thread:join()))

-- Test thread:status()
local thread = assert(apr.thread(function()
  local status, apr = pcall(require, 'apr')
  if not status then
    pcall(require, 'luarocks.require')
    apr = require 'apr'
  end
  apr.sleep(1)
end))
while assert(thread:status()) == 'init' do apr.sleep(0.1) end
assert('running' == assert(thread:status()))
assert(thread:join())
assert('done' == assert(thread:status()))
