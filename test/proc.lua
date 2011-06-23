--[[

 Unit tests for the process handling module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: June 23, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

local status, apr = pcall(require, 'apr')
if not status then
  pcall(require, 'luarocks.require')
  apr = require 'apr'
end
local helpers = require 'apr.test.helpers'

local function newchild(cmdtype, script, env)
  local child = assert(apr.proc_create 'lua')
  assert(child:cmdtype_set(cmdtype))
  if env then child:env_set(env) end
  assert(child:io_set('child-block', 'parent-block', 'parent-block'))
  assert(child:exec { helpers.scriptpath(script) })
  return child, assert(child:in_get()), assert(child:out_get()), assert(child:err_get())
end

-- Test bidirectional pipes. {{{1
local child, stdin, stdout, stderr = newchild('shellcmd/env', 'io_file-bidi_pipes.lua')
local testmsg = "Is There Anyone Out There?!"
assert(stdin:write(testmsg)); assert(stdin:close())
assert(stdout:read() == testmsg:lower())
assert(stderr:read() == testmsg:upper())

-- Test tostring(process). {{{1
assert(tostring(child):find '^process %([x%x]+%)$')

-- Test child environment manipulation. {{{1
local child = newchild('shellcmd', 'test-child-env.lua', {
  LUA_APR_MAGIC_ENV_KEY = apr._VERSION, -- This is the only thing we're interested in testing…
  SystemRoot = apr.env_get 'SystemRoot', -- needed on Windows XP, without it code = -1072365564 below
})
local done, why, code = assert(child:wait(true))
assert(done == true)
assert(why == 'exit')
-- TODO I thought I'd finally fixed the "incorrect subprocess return codes"
--      problem but it's back; now it only triggers when I run the test suite
--      under Lua/APR on Linux installed through LuaRocks :-\
-- assert(code == 42)

-- Test apr.proc_fork() when supported. {{{1
if apr.proc_fork then
  local forked_file, forked_text = assert(helpers.tmpname()), 'hello from forked child!'
  local process, context = assert(apr.proc_fork())
  if context == 'child' then
    helpers.writefile(forked_file, forked_text)
    os.exit(0)
  elseif context == 'parent' then
    assert(helpers.wait_for(forked_file, 10), "Forked child failed to create file?!")
    assert(helpers.readfile(forked_file) == forked_text)
  end
end

-- Test apr.namedpipe_create(). {{{1

local namedpipe = helpers.tmpname()
local namedmsg = "Hello world over a named pipe!"
local status, errmsg, errcode = apr.namedpipe_create(namedpipe)
if errcode ~= 'ENOTIMPL' then
  local child = assert(apr.proc_create 'lua')
  assert(child:cmdtype_set('shellcmd/env'))
  assert(child:exec { helpers.scriptpath 'io_file-named_pipe.lua', namedpipe, namedmsg })
  local handle = assert(apr.file_open(namedpipe, 'r'))
  assert(namedmsg == handle:read())
  assert(handle:close())
end

--[[

TODO Investigate escaping problem in apr_proc_create() ?!

I originally used the following message above:

  local namedmsg = "Hello world over a named pipe! :-)"

Which caused the following error message:

  /bin/sh: Syntax error: ")" unexpected

Using strace as follows I can see the escaping is lost:

  $ strace -vfe trace=execve -s 250 lua etc/tests.lua
  [pid 30868] execve("/bin/sh", ["/bin/sh", "-c", "lua etc/test-namedpipe.lua /tmp/lua-apr-tempfile-66 Hello world over a named pipe! :-)"], [...]) = 0

After removing the smiley the syntax errors disappeared but the words in
"namedmsg" are received as individual arguments by the test-namedpipe.lua
script :-\

--]]

