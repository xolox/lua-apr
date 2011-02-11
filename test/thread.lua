--[[

 Unit tests for the multi threading module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: February 11, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

local apr = require 'apr'
local helpers = require 'apr.test.helpers'

if not apr.thread_create then
  helpers.message "Not testing multi threading module (unavailable)\n"
else

  -- Check that yield() exists, can be called and does mostly nothing :-)
  assert(select('#', apr.thread_yield()) == 0)

  -- Test thread creation and argument passing.
  local threadfile = helpers.tmpname()
  local thread = assert(apr.thread_create([[
    local handle = assert(io.open(..., 'w'))
    assert(handle:write 'hello world!')
    assert(handle:close())
  ]], threadfile))
  local status, result = assert(thread:join())
  assert(result == '')

  -- Check that the file was actually created inside the thread.
  assert(helpers.readfile(threadfile) == 'hello world!')

  -- Test module loading and return value.
  local thread = assert(apr.thread_create([[
    -- Gotcha: The Lua/APR binding might be installed through
    -- LuaRocks which hasn't been initialized in this Lua state.
    pcall(require, 'luarocks.require')
    local apr = require 'apr'
    return apr.version_get()
  ]], threadfile))
  local status, result = assert(thread:join())
  assert(result == apr.version_get())

  -- Test thread:status()
  local thread = assert(apr.thread_create [[
    pcall(require, 'luarocks.require')
    local apr = require 'apr'
    apr.sleep(2)
  ]])
  apr.sleep(1)
  assert(thread:status() == 'running')
  assert(thread:join())
  assert(thread:status() == 'done')

end
