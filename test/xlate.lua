--[[

 Unit tests for the character encoding translation module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: July 3, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

local status, apr = pcall(require, 'apr')
if not status then
  pcall(require, 'luarocks.require')
  apr = require 'apr'
end
local helpers = require 'apr.test.helpers'

local utf8 = "Edelwei\195\159"
local utf7 = "Edelwei+AN8-"
local latin1 = "Edelwei\223"
local latin2 = "Edelwei\223"

-- 1. Identity transformation: UTF-8 -> UTF-8
assert(utf8 == assert(apr.xlate(utf8, 'UTF-8', 'UTF-8')))

-- 2. UTF-8 <-> ISO-8859-1
assert(latin1 == assert(apr.xlate(utf8, 'UTF-8', 'ISO-8859-1'), "(known to fail on Windows)"))
assert(utf8 == assert(apr.xlate(latin1, 'ISO-8859-1', 'UTF-8')))

-- 3. ISO-8859-1 <-> ISO-8859-2, identity
assert(latin2 == assert(apr.xlate(latin1, 'ISO-8859-1', 'ISO-8859-2')))
assert(latin1 == assert(apr.xlate(latin2, 'ISO-8859-2', 'ISO-8859-1')))

-- 4. Transformation using character set aliases
assert(utf7 == assert(apr.xlate(utf8, 'UTF-8', 'UTF-7')))
assert(utf8 == assert(apr.xlate(utf7, 'UTF-7', 'UTF-8')))
