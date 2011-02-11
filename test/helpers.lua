--[[

 Test infrastructure for the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: February 11, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

local apr = require 'apr'
local helpers = {}

function helpers.message(s, ...) -- {{{1
  io.stderr:write('\r', string.format(s, ...))
  io.stderr:flush()
end

function helpers.warning(s, ...) -- {{{1
  io.stderr:write("\nWarning: ", string.format(s, ...))
  io.stderr:flush()
end

function helpers.filedefined()
  local info = assert(debug.getinfo(2, 'S'))
  return info.source:sub(2)
end

function helpers.scriptpath(name) -- {{{1
  local directory = apr.filepath_parent(helpers.filedefined())
  return assert(apr.filepath_merge(directory, name))
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

function helpers.tmpname() -- {{{1
  local name = 'lua-apr-tempfile-' .. tmpnum
  local file = apr.filepath_merge(tmpdir, name)
  apr.file_remove(file)
  tmpnum = tmpnum + 1
  return file
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

-- }}}1

return helpers
