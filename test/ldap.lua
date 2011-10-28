--[[

 Unit tests for the LDAP connection handling module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: October 29, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

 This script executes the real test in ldap-child.lua as a child process.

--]]

local status, apr = pcall(require, 'apr')
if not status then
  pcall(require, 'luarocks.require')
  apr = require 'apr'
end
local helpers = require 'apr.test.helpers'

if not apr.ldap then
  helpers.warning "LDAP module not available!\n"
  return false
end

return helpers.ld_preload_trick 'ldap-child.lua'
