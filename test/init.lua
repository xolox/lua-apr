--[[

 Driver script for the unit tests of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: September 18, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

 Because the unit tests for the Lua/APR binding are included as installable
 modules, it's easy to run the test suite once you've installed the Lua/APR
 binding. Just execute the following command from a terminal:

   lua -e "require 'apr.test' ()"

--]]

-- TODO Cleanup and extend the tests for `filepath.c'.
-- TODO Add tests for file:seek() (tricky to get right!)
-- TODO Add tests for apr.glob()!

-- Names of modules for which tests have been written (the individual lines
-- enable automatic rebasing between git feature branches and master branch).
local modules = {
  'base64',
  'crypt',
  'date',
  'dbd',
  'dbm',
  'env',
  'filepath',
  'fnmatch',
  'getopt',
  'http',
  'io_dir',
  'io_file',
  'io_net',
  'ldap',
  'memcache',
  'misc',
  'proc',
  'shm',
  'signal',
  'str',
  'thread',
  'thread_queue',
  'time',
  'uri',
  'user',
  'uuid',
  'xlate',
  'xml'
}

local modname = ...
return function()

  local status, apr = pcall(require, 'apr')
  if not status then
    pcall(require, 'luarocks.require')
    apr = require 'apr'
  end
  local helpers = require 'apr.test.helpers'

  local success = true
  for _, testname in ipairs(modules) do
    local modname = modname .. '.' .. testname
    package.loaded[modname] = nil
    helpers.message("Running %s tests: ", testname)
    local starttime = apr.time_now()
    local status, result = pcall(require, modname)
    if status and result ~= false then
      -- All tests passed.
      local elapsed = apr.time_now() - starttime
      if elapsed >= 0.5 then
        helpers.message("OK (%.2fs)\n", elapsed)
      else
        helpers.message "OK\n"
      end
    elseif status then
      -- Soft failure (anticipated).
      helpers.message("Skipped!\n")
    else
      -- Hard failure.
      helpers.message("Failed! (%s)\n", result)
      success = false
    end
    package.loaded[modname] = nil
    -- Garbage collect unreferenced objects before testing the next module.
    collectgarbage 'collect'
    collectgarbage 'collect'
  end

  -- Cleanup temporary files.
  helpers.cleanup()

  helpers.message "Done!\n"

  -- Exit the interpreter (started with lua -e "require 'apr.test' ()").
  os.exit(success and 0 or 1)

end

-- vim: ts=2 sw=2 et
