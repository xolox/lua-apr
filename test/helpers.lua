--[[

 Test infrastructure for the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: July 2, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

local status, apr = pcall(require, 'apr')
if not status then
  pcall(require, 'luarocks.require')
  apr = require 'apr'
end
local helpers = {}

function print(...)
  local t = {}
  for i = 1, select('#', ...) do
    t[#t + 1] = tostring(select(i, ...))
  end
  io.stderr:write(table.concat(t, ' ') .. '\n')
end

function helpers.message(s, ...) -- {{{1
  io.stderr:write(string.format(s, ...))
  io.stderr:flush()
end

function helpers.warning(s, ...) -- {{{1
  io.stderr:write("\nWarning: ", string.format(s, ...))
  io.stderr:flush()
end

function helpers.try(body, errorhandler) -- {{{1
  local status, value = pcall(body)
  if status then return true end
  errorhandler(value)
  return false
end

function helpers.filedefined() -- {{{1
  local info = assert(debug.getinfo(2, 'S'))
  return info.source:sub(2)
end

function helpers.deepequal(a, b) -- {{{1
  if type(a) ~= 'table' or type(b) ~= 'table' then
    return a == b
  else
    for k, v in pairs(a) do
      if not helpers.deepequal(v, b[k]) then
        return false
      end
    end
    for k, v in pairs(b) do
      if not helpers.deepequal(v, a[k]) then
        return false
      end
    end
    return true
  end
end

function helpers.checktuple(expected, ...) -- {{{1
  assert(select('#', ...) == #expected)
  for i = 1, #expected do assert(expected[i] == select(i, ...)) end
end

local testscripts = apr.filepath_parent(helpers.filedefined())

function helpers.scriptpath(name) -- {{{1
  return assert(apr.filepath_merge(testscripts, name))
end

function helpers.ld_preload_trick(script) -- {{{1

  -- XXX This hack is needed to make the tests pass on Ubuntu 10.04 and probably
  -- also other versions of Ubuntu and Debian? The Lua/APR documentation for the
  -- DBD module contains some notes about this, here's a direct link:
  -- http://peterodding.com/code/lua/apr/docs/#debugging_dso_load_failed_errors

  -- Include the libapr-1.so and libaprutil-1.so libraries in $LD_PRELOAD if
  -- they exist in the usual Debian location.
  local libs = apr.filepath_list_split(apr.env_get 'LD_PRELOAD' or '')
  for _, libname in ipairs { '/usr/lib/libapr-1.so.0', '/usr/lib/libaprutil-1.so.0' } do
    if apr.stat(libname, 'type') == 'file' then table.insert(libs, libname) end
  end
  apr.env_set('LD_PRELOAD', apr.filepath_list_merge(libs))

  -- Now run the test in a child process where $LD_PRELOAD applies.
  local child = assert(apr.proc_create 'lua')
  assert(child:cmdtype_set 'shellcmd/env')
  assert(child:exec { helpers.scriptpath(script) })
  local dead, reason, code = assert(child:wait(true))
  return reason == 'exit' and code == 0

end

function helpers.wait_for(signalfile, timeout) -- {{{1
  local starttime = apr.time_now()
  while apr.time_now() - starttime < timeout do
    apr.sleep(0.25)
    if apr.stat(signalfile, 'type') == 'file' then
      return true
    end
  end
end

local tmpnum = 1
local tmpdir = assert(apr.temp_dir_get())

local function tmpname(tmpnum)
  return apr.filepath_merge(tmpdir, 'lua-apr-tempfile-' .. tmpnum)
end

function helpers.tmpname() -- {{{1
  local file = tmpname(tmpnum)
  apr.file_remove(file)
  tmpnum = tmpnum + 1
  return file
end

function helpers.cleanup() -- {{{1
  for i = 1, tmpnum do
    apr.file_remove(tmpname(i))
  end
end

function helpers.readfile(path) -- {{{1
  local handle = assert(io.open(path, 'r'))
  local data = assert(handle:read '*all')
  assert(handle:close())
  return data
end

function helpers.writefile(path, data) -- {{{1
  local handle = assert(io.open(path, 'w'))
  assert(handle:write(data))
  assert(handle:close())
end

function helpers.writable(directory) -- {{{1
  local entry = apr.filepath_merge(directory, 'io_dir_writable_check')
  local status = pcall(helpers.writefile, entry, 'something')
  if status then os.remove(entry) end
  return status
end

local escapes = { ['\r'] = '\\r', ['\n'] = '\\n', ['"'] = '\\"', ['\0'] = '\\0' }

function helpers.formatvalue(v) -- {{{1
  if type(v) == 'number' then
    local s = string.format('%.99f', v)
    return s:find '%.' and (s:gsub('0+$', '0')) or s
  elseif type(v) == 'string' then
    return '"' .. v:gsub('[\r\n"%z]', escapes) .. '"'
  else
    return tostring(v)
  end
end

-- }}}1

return helpers
