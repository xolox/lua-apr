--[[

 Unit tests for the environment manipulation module of the Lua/APR binding.

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

-- Based on http://svn.apache.org/viewvc/apr/apr/trunk/test/testenv.c?view=markup

local TEST_ENVVAR_NAME = "apr_test_envvar"
local TEST_ENVVAR2_NAME = "apr_test_envvar2"
local TEST_ENVVAR_VALUE = "Just a value that we'll check"

-- Test that environment variables can be set.
assert(apr.env_set(TEST_ENVVAR_NAME, TEST_ENVVAR_VALUE))

-- Test that environment variables can be read.
assert(apr.env_get(TEST_ENVVAR_NAME))
assert(apr.env_get(TEST_ENVVAR_NAME) == TEST_ENVVAR_VALUE)

-- Test that environment variables can be deleted.
assert(apr.env_delete(TEST_ENVVAR_NAME))
assert(not apr.env_get(TEST_ENVVAR_NAME))

-- http://issues.apache.org/bugzilla/show_bug.cgi?id=40764

-- Set empty string and test that status is OK.
assert(apr.env_set(TEST_ENVVAR_NAME, ""))
assert(apr.env_get(TEST_ENVVAR_NAME))
assert(apr.env_get(TEST_ENVVAR_NAME) == "")

-- Delete environment variable and retest.
assert(apr.env_delete(TEST_ENVVAR_NAME))
assert(not apr.env_get(TEST_ENVVAR_NAME))

-- Set second variable and test.
assert(apr.env_set(TEST_ENVVAR2_NAME, TEST_ENVVAR_VALUE))
assert(apr.env_get(TEST_ENVVAR2_NAME))
assert(apr.env_get(TEST_ENVVAR2_NAME) == TEST_ENVVAR_VALUE)

-- Finally, test ENOENT (first variable) followed by second != ENOENT.
assert(not apr.env_get(TEST_ENVVAR_NAME))
assert(apr.env_get(TEST_ENVVAR2_NAME))
assert(apr.env_get(TEST_ENVVAR2_NAME) == TEST_ENVVAR_VALUE)

-- Cleanup.
assert(apr.env_delete(TEST_ENVVAR2_NAME))
