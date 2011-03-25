--[[

 Unit tests for the filename matching module of the Lua/APR binding.

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

-- Check that the ?, *, and [] wild cards are supported.
assert(apr.fnmatch('lua_apr.?', 'lua_apr.c'))
assert(apr.fnmatch('lua_apr.?', 'lua_apr.h'))
assert(apr.fnmatch('lua_apr.[ch]', 'lua_apr.h'))
assert(not apr.fnmatch('lua_apr.[ch]', 'lua_apr.l'))
assert(not apr.fnmatch('lua_apr.?', 'lua_apr.cc'))
assert(apr.fnmatch('lua*', 'lua51'))

-- Check that filename matching is case sensitive by default.
assert(not apr.fnmatch('lua*', 'LUA51'))

-- Check that case insensitive filename matching works.
assert(apr.fnmatch('lua*', 'LUA51', true))

-- Check that special characters in filename matching are detected.
assert(not apr.fnmatch_test('lua51'))
assert(apr.fnmatch_test('lua5?'))
assert(apr.fnmatch_test('lua5*'))
assert(apr.fnmatch_test('[lL][uU][aA]'))
assert(not apr.fnmatch_test('+-^#@!%'))
