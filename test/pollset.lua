--[[

 Unit tests for the pollset module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: November 1, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

local status, apr = pcall(require, 'apr')
if not status then
  pcall(require, 'luarocks.require')
  apr = require 'apr'
end
local helpers = require 'apr.test.helpers'

-- Create the pollset object.
local pollset = assert(apr.pollset(10))

-- Create a server socket.
local server = assert(apr.socket_create())
assert(server:listen(10))

-- Add the server socket to the pollset.
assert(pollset:add(server, 'input'))

-- vim: ts=2 sw=2 et tw=79 fen fdm=marker
