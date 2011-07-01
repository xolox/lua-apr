--[[

 Unit tests for the multi threading module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: July 1, 2011
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

-- Test thread creation and argument passing.
local threadfile = helpers.tmpname()
local thread = apr.thread(function(threadfile)
  local handle = assert(io.open(threadfile, 'w'))
  assert(handle:write 'hello world!')
  assert(handle:close())
end, threadfile)
assert(thread:join())

-- Check that the file was actually created inside the thread.
assert(helpers.readfile(threadfile) == 'hello world!')

-- Test module loading and multiple return values.
local thread = apr.thread(function()
  local status, apr = pcall(require, 'apr')
  if not status then
    pcall(require, 'luarocks.require')
    apr = require 'apr'
  end
  return apr.version_get().apr
end)
helpers.checktuple({ true, apr.version_get().apr }, assert(thread:join()))

-- Test thread:status()
local thread = apr.thread(function()
  local status, apr = pcall(require, 'apr')
  if not status then
    pcall(require, 'luarocks.require')
    apr = require 'apr'
  end
  apr.sleep(1)
end)
while assert(thread:status()) == 'init' do apr.sleep(0.1) end
assert('running' == assert(thread:status()))
assert(thread:join())
assert('done' == assert(thread:status()))
