--[[

 Unit tests for the user/group identification module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: February 11, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

local apr = require 'apr'

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
  assert(apr.user_homepath_get(user) == env_home)
end
