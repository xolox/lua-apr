#!/usr/bin/env lua

--[[

 Multi platform build bot for the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: June 16, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

 This script makes it easier for me to build and test the Lua/APR binding on
 multiple platforms simultaneously. In its simplest form this script builds the
 Lua/APR binding, copies the installation files to a temporary directory and
 runs the test suite inside this temporary directory. The script can also be
 started in server mode where it will wait for another build bot that commands
 it over TCP to run a build and report the results.

 I start this script in server mode in a Windows XP virtual machine, then more
 or less forget about the virtual machine and just run the build bot from my
 Ubuntu Linux host environment, where it reports the build and test results
 from both my Linux and Windows environments :-)

 The following issues influenced the design of this build bot (I came up with
 these after finding out the hard way every variation that didn't work out):

  * On Windows the build bot can't run "make install" because the build bot
    itself uses the Lua/APR binding and on Windows it is impossible to
    overwrite dynamic link libraries while they're in use :-\

  * On UNIX the build bot can be run from a low privilege account because "make
    install" is no longer needed. This makes it easier to run the build bot
    from a web hook (for example)

 Note that this script uses the Lua/APR binding so you'll need to run "make
 install" or similar at least once before trying to run this script.

--]]

local apr = require 'apr'
local serverport = 38560
local projectdir = assert(apr.filepath_get())
local builddir = projectdir .. '/build'
local libext = apr.platform_get() == 'WIN32' and 'dll' or 'so'
local libname = 'core.' .. libext
local buildfiles = {
  [libname] = 'apr/' .. libname,
  ['src/apr.lua'] = 'apr.lua',
  ['test'] = 'apr/test',
}
local usage = [[
Usage: buildbot [OPTS]

  --local      build and test on this machine
  --remote=IP  build and test on a remote machine
  --server     become a server, waiting for commands
  --port=NR    port number for server build bot
]]

function main() -- {{{1
  local opts, args = apr.getopt(usage)
  local did_something = false
  if opts['local'] then
    for line in local_build() do
      printf('%s', line)
    end
    did_something = true
  end
  if opts.remote then
    remote_build(opts.remote, serverport or opts.port)
    did_something = true
  end
  if opts.server then
    start_server(serverport or opts.port)
    did_something = true
  end
  if not did_something then
    io.write(usage)
  end
end

function local_build() -- {{{1
  -- Create a binary for the current platform using a makefile.
  return coroutine.wrap(function()
    local started = apr.time_now()
    local platform = apr.platform_get()
    coroutine.yield("Building on " .. platform .. " ..")
    local build = assert(io.popen(platform ~= 'WIN32' and 'make --no-print-directory 2>&1'
        or 'nmake /nologo /f Makefile.win 2>&1'))
    for line in build:lines() do
      coroutine.yield(line)
    end
    create_env()
    run_tests()
    printf("\nFinished local build in %.2f seconds!\n",
        apr.time_now() - started)
  end)
end

function create_env() -- {{{1
  -- Create or update the virtual environment.
  for source, target in pairs(buildfiles) do
    source = projectdir .. '/' .. source
    target = builddir .. '/' .. target
    if apr.stat(source, 'type') ~= 'directory' then
      copy_file(source, target)
    else
      local directory = assert(apr.dir_open(source))
      for entry in directory:entries 'name' do
        if entry:find '%.lua$' then
          copy_file(source .. '/' .. entry, target .. '/' .. entry)
        end
      end
      assert(directory:close())
    end
  end
end

function create_dir(path) -- {{{2
  -- Create directories in virtual environment automatically.
  if apr.stat(path, 'type') ~= 'directory' then
    assert(apr.dir_make_recursive(path))
  end
end

function copy_file(source, target) -- {{{2
  -- Copy changed files to the virtual environment.
  if apr.stat(source, 'mtime') ~= apr.stat(target, 'mtime') then
    create_dir(apr.filepath_parent(target))
    assert(apr.file_copy(source, target))
  end
end

function run_tests() -- {{{1
  -- Run the test suite in the virtual environment.
  local pathsep = package.config:sub(3, 3)
  assert(apr.env_set('LUA_PATH', builddir .. '/?.lua' .. pathsep .. builddir .. '/?/init.lua'))
  assert(apr.env_set('LUA_CPATH', builddir .. '/?.' .. libext))
  assert(apr.env_set('LD_PRELOAD', '/lib/libSegFault.so'))
  local testsuite = assert(io.popen [[ lua -e "require 'apr.test' ()" 2>&1 ]])
  for line in testsuite:lines() do
    coroutine.yield(line)
  end
  assert(testsuite:close())
end

function start_server(port) -- {{{1
  -- Open a TCP server socket and wait for commands from another build bot.
  local server = assert(apr.socket_create('tcp', 'inet'))
  local localhost = apr.host_to_addr(apr.hostname_get())
  assert(server:bind('*', port))
  assert(server:listen(1))
  while true do
    printf("Build bot listening on %s:%i ..", localhost, port)
    local client = assert(server:accept())
    for line in local_build() do
      printf('%s', line)
      client:write(line, '\n')
    end
    assert(client:close())
    printf "Finished build .."
  end
end

function remote_build(host, port) -- {{{1
  -- Command a remote build bot to perform a build and report its results.
  local started = apr.time_now()
  local socket = assert(apr.socket_create('tcp', 'inet'))
  assert(socket:connect(host, port))
  for line in socket:lines() do
    printf('%s', line)
  end
  assert(socket:close())
  printf("\nFinished remote build in %.2f seconds!\n",
      apr.time_now() - started)
end

function printf(...) -- {{{1
  io.stderr:write(string.format(...), '\n')
  io.stderr:flush()
end

-- }}}

main()
