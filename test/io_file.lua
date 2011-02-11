--[[

 Unit tests for the file I/O handling module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: February 11, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

local apr = require 'apr'
local helpers = require 'apr.test.helpers'
local selfpath = helpers.filedefined()
local testdata = [[
 1
 3.1
 100
 0xCAFEBABE
 0xDEADBEEF
 3.141592653589793115997963468544185161590576171875
 this line is in fact not a number :-)

 that was an empty line
]]

-- Test the stat() function. {{{2
local info = apr.stat(selfpath)
assert(type(info) == 'table')
assert(info.name == 'io_file.lua')
assert(info.path:find 'io_file%.lua$')
assert(info.type == 'file')
assert(info.size >= 1024 * 7)
assert(info.mtime >= 1293510503)
assert(info.nlink >= 1)
assert(type(info.inode) == 'number')
assert(type(info.dev) == 'number')
assert(info.protection:find '^[-r][-w][-xSs][-r][-w][-xSs][-r][-w][-xTt]$')

-- Test the alternative stat() interface. {{{2
assert(apr.stat(selfpath, 'type') == 'file')
assert(apr.stat(selfpath, 'name') == 'io_file.lua')
local selfdir = apr.filepath_parent(selfpath)
local kind, size, prot = apr.stat(selfdir, 'type', 'size', 'protection')
assert(kind == 'directory')
assert(type(size) == 'number')
assert(prot:find '^[-r][-w][-xSs][-r][-w][-xSs][-r][-w][-xTt]$')

-- Test apr.file_perms_set().  {{{2
local tempname = assert(helpers.tmpname())
helpers.writefile(tempname, 'something')
local status, errmsg, errcode = apr.file_perms_set(tempname, 'rw-rw----')
if errcode ~= 'ENOTIMPL' then
  assert(apr.stat(tempname, 'protection') == 'rw-rw----')
  assert(apr.file_perms_set(tempname, 'ug=r,o='))
  assert(apr.stat(tempname, 'protection') == 'r--r-----')
end

-- Test apr.file_copy(). {{{2
local copy1 = assert(helpers.tmpname())
helpers.writefile(copy1, testdata)
local copy2 = assert(helpers.tmpname())
assert(apr.file_copy(copy1, copy2))
assert(testdata == helpers.readfile(copy2))

-- Test apr.file_append(). {{{2
assert(apr.file_append(copy1, copy2))
assert(helpers.readfile(copy2) == testdata:rep(2))

-- Test apr.file_rename(). {{{2
assert(apr.file_rename(copy1, copy2))
assert(not apr.stat(copy1))
assert(helpers.readfile(copy2) == testdata)

-- Test apr.file_mtime_set(). {{{2
local mtime = math.random(0, apr.time_now())
assert(apr.stat(copy2, 'mtime') ~= mtime)
assert(apr.file_mtime_set(copy2, mtime))
assert(apr.stat(copy2, 'mtime') == mtime)

-- Test apr.file_attrs_set(). {{{2
local status, errmsg, errcode = apr.file_perms_set(copy2, 'ug=rw,o=')
if errcode ~= 'ENOTIMPL' then
  assert(apr.stat(copy2, 'protection'):find '^.w..w....$')
  assert(apr.file_attrs_set(copy2, { readonly=true }))
  assert(apr.stat(copy2, 'protection'):find '^.[^w]..[^w]....$')
end

-- Test file:stat(). {{{2
local handle = assert(apr.file_open(copy2))
assert(handle:stat('type') == 'file')
assert(handle:stat('size') >= #testdata)

-- Test file:lines(). {{{2
local lines = {}
for line in assert(handle:lines()) do lines[#lines + 1] = line end
for line in testdata:gmatch '[^\n]+' do
  local otherline = table.remove(lines, 1)
  if otherline == '' then
    -- the pattern above ignores the empty line
    otherline = table.remove(lines, 1)
  end
  assert(line == otherline)
end
assert(#lines == 0)

-- Test tostring(file). {{{2
assert(tostring(handle):find '^file %([x%x]+%)$')
assert(handle:close())
assert(tostring(handle):find '^file %(closed%)$')

-- Test file:read(), file:write() and file:seek() {{{2

local maxmultiplier = 20
for testsize = 1, maxmultiplier do

  local testdata = testdata:rep(testsize)
  -- message("Testing file I/O on a file of %i bytes (step %i/%i)..\n", #testdata, testsize, maxmultiplier)
  local testlines = {}
  for l in testdata:gmatch '[^\r\n]*' do
    table.insert(testlines, l)
  end

  -- Create temporary file with test data.
  local tempname = assert(helpers.tmpname())
  local testfile = assert(io.open(tempname, 'w'))
  assert(testfile:write(testdata))
  assert(testfile:close())

  -- Read back the temporary file's contents using various formats.
  local escapes = { ['\r'] = '\\r', ['\n'] = '\\n', ['"'] = '\\"', ['\0'] = '\\0' }
  local function formatvalue(v)
    if type(v) == 'number' then
      local s = fmt('%.99f', v)
      return s:find '%.' and (s:gsub('0+$', '0')) or s
    elseif type(v) == 'string' then
      return '"' .. v:gsub('[\r\n"%z]', escapes) .. '"'
      -- XXX return fmt('%q', v)
    else
      return tostring(v)
    end
  end

  local function testformat(format)
    helpers.message("Testing file:read(%s) ..", format)
    local apr_file = assert(apr.file_open(tempname, 'r'))
    local lua_file = assert(io.open(tempname, 'r'))
    repeat
      local lua_value = lua_file:read(format)
      local apr_value = apr_file:read(format)
      if lua_value ~= apr_value then
        helpers.warning("Wrong result for file:read(%q)!\nLua value: %s\nAPR value: %s\n",
            format, formatvalue(lua_value), formatvalue(apr_value))
        helpers.warning("Lua position: %i, APR position: %i", lua_file:seek 'cur', apr_file:seek 'cur')
        helpers.warning("Remaining data in Lua file: %s", formatvalue(lua_file:read '*a' :sub(1, 50)))
        helpers.warning("Remaining data in APR file: %s", formatvalue(apr_file:read '*a' :sub(1, 50)))
        os.exit(1)
      end
      assert(lua_value == apr_value)
    until (format == '*a' and lua_value == '') or not lua_value
    assert(lua_file:close())
    assert(apr_file:close())
  end

  testformat '*n'
  testformat '*l'
  testformat '*a'
  testformat (1)
  testformat (2)
  testformat (3)
  testformat (4)
  testformat (5)
  testformat (10)
  testformat (20)
  testformat (50)
  testformat (100)

  assert(os.remove(tempname))

  -- Perform write tests.

  local lua_file = assert(helpers.tmpname())
  local apr_file = assert(helpers.tmpname())

  local function write_all(lua_handle, apr_handle)
    assert(lua_handle:write(testdata))
    assert(apr_handle:write(testdata))
  end

  local function write_blocks(lua_handle, apr_handle)
    local i = 1
    local bs = 1
    repeat
      assert(lua_handle:write(testdata:sub(i, i + bs)))
      assert(apr_handle:write(testdata:sub(i, i + bs)))
      i = i + bs + 1
      bs = bs * 2
    until i > #testdata
  end

  local function write_lines(lua_handle, apr_handle)
    for _, line in ipairs(testlines) do
      assert(lua_handle:write(line, '\n'))
      assert(apr_handle:write(line, '\n'))
    end
  end

  for i, mode in ipairs { 'w', 'wb' } do
    for j, writemethod in ipairs { write_all, write_blocks, write_lines } do
      local lua_handle = assert(io.open(lua_file, mode)) -- TODO test b mode
      local apr_handle = assert(apr.file_open(apr_file, mode))
      helpers.message("Testing file:write() mode %i, method %i ..", i, j)
      writemethod(lua_handle, apr_handle)
      assert(lua_handle:close())
      assert(apr_handle:close())
      for _, format in ipairs { '*l', '*a' } do
        lua_handle = assert(io.open(lua_file, 'r'))
        apr_handle = assert(apr.file_open(apr_file, 'r'))
        while true do
          local lua_value = lua_handle:read(format)
          local apr_value = apr_handle:read(format)
          if lua_value ~= apr_value then
            helpers.warning("Buggy output from file:write()?!\nLua value: %s\nAPR value: %s\n",
                formatvalue(lua_value), formatvalue(apr_value))
          end
          assert(lua_value == apr_value)
          if not lua_value or (format == '*a' and lua_value == '') then break end
        end
        assert(lua_handle:close())
        assert(apr_handle:close())
      end
    end
  end

  assert(os.remove(lua_file))
  assert(os.remove(apr_file))

end
