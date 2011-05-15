--[[

 Unit tests for the thread queues module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: May 15, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

 This script runs the thread queue tests in a child process to
 protect the test suite from crashing on unsupported platforms.

--]]

local status, apr = pcall(require, 'apr')
if not status then
  pcall(require, 'luarocks.require')
  apr = require 'apr'
end
local helpers = require 'apr.test.helpers'

if not apr.thread_queue then
  helpers.warning "Thread queues module not available!\n"
  return false
end

local child = assert(apr.proc_create 'lua')
assert(child:cmdtype_set 'shellcmd/env')
assert(child:exec { helpers.scriptpath 'thread_queue-child.lua' })
local dead, reason, code = assert(child:wait(true))
return reason == 'exit' and code == 0
