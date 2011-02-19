--[[

 Unit tests for the thread queues module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: February 19, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

local apr = require 'apr'
local helpers = require 'apr.test.helpers'

if not apr.thread_queue then
  helpers.warning "Thread queues module not available!\n"
  return false
end

-- Create a thread queue with space for one tuple.
local queue = assert(apr.thread_queue(1))

-- Test that the queue starts empty.
assert(not queue:trypop())

-- Pass the thread queue to a thread.
local thread = assert(apr.thread_create([[
  pcall(require, 'luarocks.require')
  local apr = require 'apr'
  local queue = ...
  -- Scalar values.
  assert(queue:push(nil))
  assert(queue:push(false))
  assert(queue:push(true))
  assert(queue:push(42))
  assert(queue:push(math.pi))
  assert(queue:push "hello world through a queue!")
  -- Tuples.
  assert(queue:push(true, false, 13, math.huge, _VERSION))
  -- Object values.
  assert(queue:push(queue))
  assert(queue:push(apr.pipe_open_stdin()))
  assert(queue:push(apr.socket_create()))
]], queue))

-- Check the sequence of supported value types.
assert(queue:pop() == nil)
assert(queue:pop() == false)
assert(queue:pop() == true)
assert(queue:pop() == 42)
assert(queue:pop() == math.pi)
assert(queue:pop() == "hello world through a queue!")

-- Check that multiple values are supported.
local expected = { true, false, 13, math.huge, _VERSION }
helpers.checktuple(expected, assert(queue:pop()))

-- These test that Lua/APR objects can be passed between threads and that
-- objects which are really references __equal the object they reference.
assert(assert(queue:pop()) == queue)
assert(apr.type(queue:pop()) == 'file')
assert(apr.type(queue:pop()) == 'socket')

-- Now make sure the queue is empty again.
assert(not queue:trypop())

-- Make sure trypush() works as expected.
assert(queue:push(1)) -- the thread queue is now full
assert(not queue:trypush(2)) -- thus trypush() should fail
