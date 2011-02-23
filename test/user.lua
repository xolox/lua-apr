--[[

 Unit tests for the user/group identification module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: February 23, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

local apr = require 'apr'
local helpers = require 'apr.test.helpers'

-- First check whether apr.user_get() works or returns an error.
assert(apr.user_get())

local user, group = assert(apr.user_get())

-- Get the name of the current user.
assert(user)

-- Get primary group of current user.
assert(group)

local env_user = apr.env_get 'USER'
if env_user then
  -- Match result of apr.user_get() against $USER.
  assert(user == env_user)
end

local env_home = apr.env_get 'HOME'
if env_home then
  -- Match result of apr.user_homepath_get() against $HOME.
  local apr_home = assert(apr.user_homepath_get(user))
  if apr_home ~= env_home then
    helpers.warning("$HOME == %q while apr.user_homepath_get(%q) == %q (you probably changed $HOME?)\n", env_home, user, apr_home)
  end
end
