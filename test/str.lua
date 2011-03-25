--[[

 Unit tests for the string routines module of the Lua/APR binding.

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

-- Test natural comparison.
local filenames = { 'rfc2086.txt', 'rfc1.txt', 'rfc822.txt' }
table.sort(filenames, apr.strnatcmp)
assert(filenames[1] == 'rfc1.txt')
assert(filenames[2] == 'rfc822.txt')
assert(filenames[3] == 'rfc2086.txt')

-- Test case insensitive natural comparison.
local filenames = { 'RFC2086.txt', 'RFC1.txt', 'rfc822.txt' }
table.sort(filenames, apr.strnatcasecmp)
assert(filenames[1] == 'RFC1.txt')
assert(filenames[2] == 'rfc822.txt')
assert(filenames[3] == 'RFC2086.txt')

-- Test binary size formatting.
assert(apr.strfsize(1024^1) == '1.0K')
assert(apr.strfsize(1024^2) == '1.0M')
assert(apr.strfsize(1024^3) == '1.0G')

-- Test command line tokenization.
local command = [[ program argument1 "argument 2" 'argument 3' argument\ 4 ]]
local cmdline = assert(apr.tokenize_to_argv(command))
assert(cmdline[1] == 'program')
assert(cmdline[2] == 'argument1')
assert(cmdline[3] == 'argument 2')
assert(cmdline[4] == 'argument 3')
assert(cmdline[5] == 'argument 4')
