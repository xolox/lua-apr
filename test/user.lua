--[[

 Unit tests for the user/group identification module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: March 27, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

 Note that these tests don't assert() on anything useful because that would
 make it impossible to run the Lua/APR test suite in "chrooted" build
 environments and such..

--]]

local status, apr = pcall(require, 'apr')
if not status then
  pcall(require, 'luarocks.require')
  apr = require 'apr'
end
local helpers = require 'apr.test.helpers'

-- Get the name and primary group of the current user.
local apr_user, apr_group = assert(apr.user_get())
assert(type(apr_user) == 'string' and apr_user ~= '')
assert(type(apr_group) == 'string' and apr_group ~= '')

local function report(...)
  helpers.warning(...)
  helpers.message("This might not be an error, e.g. when using a chroot, which is why the tests will continue as normal.\n")
end

-- Try to match the result of apr.user_get() against $USER.
local env_user = apr.env_get 'USER' or apr.env_get 'USERNAME' or ''
if apr_user ~= env_user then
  report("$USER == %q but apr.user_get() == %q\n", env_user, apr_user)
  return false
end

-- Try to match the result of apr.user_homepath_get() against $HOME.
local function normpath(p)
  return assert(apr.filepath_merge('', p, 'native'))
end
local env_home = normpath(apr.env_get 'HOME' or apr.env_get 'USERPROFILE' or '')
local apr_home = normpath(assert(apr.user_homepath_get(apr_user)))
if apr_home ~= env_home then
  report("$HOME == %q but apr.user_homepath_get(%q) == %q\n", env_home, apr_user, apr_home)
  return false
end
