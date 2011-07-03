--[[

 Unit tests for the signal handling module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: July 3, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

 I wanted to write this test as follows: Start a child process that listens for
 SIGKILL and/or SIGQUIT, kill the child process using process:kill() and check
 that the child process received the signals. Unfortunately it doesn't...

 The only other useful way I could think of to test that signals work is what
 I've now implemented in this test: Listen for SIGCHLD, start and then kill a
 child process and check that the parent process received SIGCHLD.

--]]

local status, apr = pcall(require, 'apr')
if not status then
  pcall(require, 'luarocks.require')
  apr = require 'apr'
end
local helpers = require 'apr.test.helpers'

-- The ISO C standard only requires the signal names SIGABRT, SIGFPE, SIGILL,
-- SIGINT, SIGSEGV, and SIGTERM to be defined (and these are in fact the only
-- signal names defined on Windows).
local names = apr.signal_names()
assert(names.SIGABRT)
assert(names.SIGFPE)
assert(names.SIGILL)
assert(names.SIGINT)
assert(names.SIGSEGV)
assert(names.SIGTERM)

-- apr.signal() tests using apr.signal_raise(). {{{1

local got_sigabrt = false
apr.signal('SIGABRT', function()
  got_sigabrt = true
end)
assert(apr.signal_raise 'SIGABRT')
assert(got_sigabrt)

local got_sigterm = false
apr.signal('SIGTERM', function()
  got_sigterm = true
end)
assert(apr.signal_raise 'SIGTERM')
assert(got_sigterm)

-- apr.signal() tests using real signals. {{{1

if apr.platform_get() ~= 'WIN32' then

  -- Spawn a child process that dies.
  local function spawn()
    local child = assert(apr.proc_create 'lua')
    assert(child:cmdtype_set 'program/env/path')
    assert(child:exec { '-e', 'os.exit(0)' })
    assert(child:wait(true))
  end

  -- Use apr.signal() to listen for dying child processes.
  local got_sigchild = false
  apr.signal('SIGCHLD', function()
    got_sigchild = true
  end)

    -- Create and kill a child process.
    spawn()

    -- Make sure we got the signal.
    assert(got_sigchild)

  -- Test that apr.signal_block() blocks the signal.
  got_sigchild = false
  assert(apr.signal_block 'SIGCHLD')

    -- Create and kill a child process.
    spawn()

    -- Make sure we didn't get the signal.
    assert(not got_sigchild)

  -- Test that apr.signal_unblock() unblocks the signal.
  assert(apr.signal_unblock 'SIGCHLD')

    -- Create and kill a child process.
    spawn()

    -- Make sure we got the signal.
    assert(got_sigchild)

  -- Test that signal handlers can be disabled.
  apr.signal('SIGCHLD', nil)
  got_sigchild = false

    -- Create and then kill a child process.
    spawn()

    -- Make sure the old signal handler was not executed.
    assert(not got_sigchild)

end
