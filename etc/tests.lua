--[[

 Test suite for the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: December 31, 2010
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

local apr = assert(require 'apr')

-- TODO Cleanup and extend the tests for `filepath.c'.
-- TODO Add tests for file:seek() (tricky to get right!)
-- TODO Add tests for apr.glob()!

-- Test infrastructure {{{1

local function message(s, ...)
  io.stderr:write('\r', s:format(...))
  io.stderr:flush()
end

local function warning(s, ...)
  io.stderr:write("\nWarning: ", s:format(...))
  io.stderr:flush()
end

do
  local tmpnum = 1
  local tmpdir = assert(apr.temp_dir_get())
  function os.tmpname()
    local name = 'lua-apr-tempfile-' .. tmpnum
    local file = apr.filepath_merge(tmpdir, name)
    apr.file_remove(file)
    tmpnum = tmpnum + 1
    return file
  end
end

-- Base64 encoding module (base64.c) {{{1
message "Testing base64 encoding ..\n"

-- Sample data from http://en.wikipedia.org/wiki/Base64#Examples
local plain = 'Man is distinguished, not only by his reason, but by this singular passion from other animals, which is a lust of the mind, that by a perseverance of delight in the continued and indefatigable generation of knowledge, exceeds the short vehemence of any carnal pleasure.'
local coded = 'TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB0aGlzIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcyBhIGx1c3Qgb2YgdGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0aGUgY29udGludWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZGdlLCBleGNlZWRzIHRoZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3VyZS4='

-- Check that Base64 encoding returns the expected result.
assert(apr.base64_encode(plain) == coded)

-- Check that Base64 decoding returns the expected result.
assert(apr.base64_decode(coded) == plain)

-- Cryptography module (crypt.c) {{{1
message "Testing cryptography module ..\n"

-- Sample data from http://en.wikipedia.org/wiki/MD5#MD5_hashes
-- and http://en.wikipedia.org/wiki/SHA1#Example_hashes

-- Check that MD5 hashing returns the expected result.
assert(apr.md5 '' == 'd41d8cd98f00b204e9800998ecf8427e')
assert(apr.md5 'The quick brown fox jumps over the lazy dog' == '9e107d9d372bb6826bd81d3542a419d6')
assert(apr.md5 'The quick brown fox jumps over the lazy eog' == 'ffd93f16876049265fbaef4da268dd0e')

pass, salt = 'password', 'salt'
hash = apr.md5_encode(pass, salt)
-- Test that MD5 passwords can be validated.
assert(apr.password_validate(pass, hash))

-- Test that SHA1 hashing returns the expected result.
assert(apr.sha1 'The quick brown fox jumps over the lazy dog' == '2fd4e1c67a2d28fced849ee1bb76e7391b93eb12')
assert(apr.sha1 'The quick brown fox jumps over the lazy cog' == 'de9f2c7fd25e1b3afad3e85a0bd17d9b100db4b3')

-- Date parsing module (date.c) {{{1
message "Testing date parsing module ..\n"

-- The following tests were copied from http://svn.apache.org/viewvc/apr/apr/trunk/test/testdate.c?view=markup
for i, rfc_pair in ipairs {
  { "Mon, 27 Feb 1995 20:49:44 -0800",  "Tue, 28 Feb 1995 04:49:44 GMT" },
  { "Fri,  1 Jul 2005 11:34:25 -0400",  "Fri, 01 Jul 2005 15:34:25 GMT" },
  { "Monday, 27-Feb-95 20:49:44 -0800", "Tue, 28 Feb 1995 04:49:44 GMT" },
  { "Tue, 4 Mar 1997 12:43:52 +0200",   "Tue, 04 Mar 1997 10:43:52 GMT" },
  { "Mon, 27 Feb 95 20:49:44 -0800",    "Tue, 28 Feb 1995 04:49:44 GMT" },
  { "Tue,  4 Mar 97 12:43:52 +0200",    "Tue, 04 Mar 1997 10:43:52 GMT" },
  { "Tue, 4 Mar 97 12:43:52 +0200",     "Tue, 04 Mar 1997 10:43:52 GMT" },
  { "Mon, 27 Feb 95 20:49 GMT",         "Mon, 27 Feb 1995 20:49:00 GMT" },
  { "Tue, 4 Mar 97 12:43 GMT",          "Tue, 04 Mar 1997 12:43:00 GMT" }
} do
  local time = assert(apr.date_parse_rfc(rfc_pair[1]))
  assert(apr.time_format('rfc822', time) == rfc_pair[2])
end

-- The tests for apr_date_parse_http() are far too complex to reproduce in Lua
-- so instead here's a round trip of one of the examples in the documentation:
local date = 'Sun, 06 Nov 1994 08:49:37 GMT'
assert(apr.time_format('rfc822', apr.date_parse_http(date)) == date)

-- DBM routines module (dbm.c) {{{1
message "Testing DBM module ..\n"

local dbmfile = os.tmpname()
local dbm = assert(apr.dbm_open(dbmfile, 'n'))
local dbmkey, dbmvalue = 'the key', 'the value'
assert(not dbm:firstkey()) -- nothing there yet
assert(dbm:store(dbmkey, dbmvalue))
local function checkdbm()
  assert(dbm:exists(dbmkey))
  assert(dbm:fetch(dbmkey) == dbmvalue)
  assert(dbm:firstkey() == dbmkey)
  assert(not dbm:nextkey(dbmkey)) -- only 1 record exists
end
checkdbm()
assert(dbm:close())
local file1, file2 = assert(apr.dbm_getnames(dbmfile))
assert(apr.stat(file1, 'type') == 'file')
assert(not file2 or apr.stat(file2, 'type') == 'file')
dbm = assert(apr.dbm_open(dbmfile, 'w'))
checkdbm()
assert(dbm:delete(dbmkey))
print(dbm:fetch(dbmkey))
assert(not dbm:fetch(dbmkey))
assert(not dbm:firstkey())
-- Test tostring(dbm).
assert(tostring(dbm):find '^dbm %([x%x]+%)$')
assert(dbm:close())
assert(tostring(dbm):find '^dbm %(closed%)$')

-- Environment manipulation module (env.c) {{{1
message "Testing environment manipulation ..\n"

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

-- File path manipulation module (filepath.c) {{{1
message "Testing file path manipulation ..\n"

-- Test apr.filepath_name()
assert(apr.filepath_name('/usr/bin/lua') == 'lua')
local parts = { apr.filepath_name('/home/xolox/.vimrc', true) }
assert(#parts == 2 and parts[1] == '.vimrc' and parts[2] == '')
parts = { apr.filepath_name('index.html.en', true) }
assert(#parts == 2 and parts[1] == 'index.html' and parts[2] == '.en')

-- Test apr.filepath_root()
if apr.platform_get() == 'WIN32' then
  assert(apr.filepath_root 'c:/foo/bar' == 'c:/')
else
  -- Doesn't really do anything useful on UNIX :-P
  assert(apr.filepath_root '/foo/bar' == '/')
end

-- Test apr.filepath_merge()
if apr.platform_get() == 'WIN32' then
  assert(apr.filepath_merge('c:/', 'foo') == 'c:/foo')
else
  assert(apr.filepath_merge('/foo', 'bar') == '/foo/bar')
end

-- Based on http://svn.apache.org/viewvc/apr/apr/trunk/test/testpath.c?view=markup.

local PSEP, DSEP
local p = apr.platform_get()
if p == 'WIN32' or p == 'NETWARE' or p == 'OS2' then
	PSEP, DSEP = ';', '\\'
else
	PSEP, DSEP = ':', '/'
end
local PX = ""
local P1 = "first path"
local P2 = "second" .. DSEP .. "path"
local P3 = "th ird" .. DSEP .. "path"
local P4 = "fourth" .. DSEP .. "pa th"
local P5 = "fifthpath"
local parts_in = { P1, P2, P3, PX, P4, P5 }
local path_in = table.concat(parts_in, PSEP)
local parts_out = { P1, P2, P3, P4, P5 }
local path_out = table.concat(parts_out, PSEP)

-- list_split_multi
do
  local pathelts = assert(apr.filepath_list_split(path_in))
  assert(#parts_out == #pathelts)
  for i = 1, #pathelts do assert(parts_out[i] == pathelts[i]) end
end

-- list_split_single
for i = 1, #parts_in do
	local pathelts = assert(apr.filepath_list_split(parts_in[i]))
	if parts_in[i] == '' then
		assert(#pathelts == 0)
	else
	assert(#pathelts == 1)
		assert(parts_in[i] == pathelts[1])
	end
end

-- list_merge_multi
do
  local pathelts = {}
  for i = 1, #parts_in do pathelts[i] = parts_in[i] end
  local liststr = assert(apr.filepath_list_merge(pathelts))
  assert(liststr == path_out)
end

-- list_merge_single
for i = 1, #parts_in do
	local liststr = assert(apr.filepath_list_merge{ parts_in[i] })
	if parts_in[i] == '' then
		assert(liststr == '')
	else
		assert(liststr == parts_in[i])
	end
end

-- Filename matching module (fnmatch.c) {{{1
message "Testing filename matching ..\n"

-- Check that the ?, *, and [] wild cards are supported.
assert(apr.fnmatch('lua_apr.?', 'lua_apr.c'))
assert(apr.fnmatch('lua_apr.?', 'lua_apr.h'))
assert(apr.fnmatch('lua_apr.[ch]', 'lua_apr.h'))
assert(not apr.fnmatch('lua_apr.[ch]', 'lua_apr.l'))
assert(not apr.fnmatch('lua_apr.?', 'lua_apr.cc'))
assert(apr.fnmatch('lua*', 'lua51'))

-- Check that filename matching is case sensitive by default.
assert(not apr.fnmatch('lua*', 'LUA51'))

-- Check that case insensitive filename matching works.
assert(apr.fnmatch('lua*', 'LUA51', true))

-- Check that special characters in filename matching are detected.
assert(not apr.fnmatch_test('lua51'))
assert(apr.fnmatch_test('lua5?'))
assert(apr.fnmatch_test('lua5*'))
assert(apr.fnmatch_test('[lL][uU][aA]'))
assert(not apr.fnmatch_test('+-^#@!%'))

-- Directory manipulation module (io_dir.c) {{{1
message "Testing directory manipulation ..\n"

local function readfile(path)
  local handle = assert(io.open(path, 'r'))
  local data = assert(handle:read '*all')
  assert(handle:close())
  return data
end

local function writefile(path, data)
  local handle = assert(io.open(path, 'w'))
  assert(handle:write(data))
  assert(handle:close())
end

local function writable(directory)
  local entry = apr.filepath_merge(directory, 'io_dir_writable_check')
  if pcall(writefile, entry, 'something') then
    os.remove(entry)
    return true
  end
end

-- Make sure apr.temp_dir_get() returns an existing, writable directory
assert(writable(assert(apr.temp_dir_get())))

-- Create a temporary workspace directory for the following tests
assert(apr.dir_make 'io_dir_tests')

-- Change to the temporary workspace directory
local cwd_save = assert(apr.filepath_get())
assert(apr.filepath_set 'io_dir_tests')

-- Test dir_make()
assert(apr.dir_make 'foobar')
assert(writable 'foobar')

-- Test dir_remove()
assert(apr.dir_remove 'foobar')
assert(not writable 'foobar')

-- Test dir_make_recursive() and dir_remove_recursive()
assert(apr.dir_make_recursive 'foo/bar/baz')
assert(writable 'foo/bar/baz')
assert(apr.dir_remove_recursive 'foo')
assert(not writable 'foo')

-- Random selection of Unicode from my music library :-)
local nonascii = {
  'Mindless Self Indulgence - Despierta Los Niños',
  'Manu Chao - Próxima Estación; Esperanza',
  'Cassius - Au Rêve',
  '菅野 よう子',
}

for _, name in ipairs(nonascii) do
  assert(apr.dir_make(name))
  -- Using writable() here won't work because the APR API deals with UTF-8
  -- while the Lua API does not, which makes the strings non-equal... :-)
  assert(assert(apr.stat(name, 'type')) == 'directory')
end

-- Test apr.dir_open() and directory methods.
local dir = assert(apr.dir_open '.')
local entries = {}
for name in dir:entries 'name' do entries[#entries+1] = name end
assert(dir:rewind())
local rewinded = {}
for name in dir:entries 'name' do rewinded[#rewinded+1] = name end
-- Test tostring(directory)
assert(tostring(dir):find '^directory %([x%x]+%)$')
assert(dir:close())
assert(tostring(dir):find '^directory %(closed%)$')
table.sort(nonascii)
table.sort(entries)
table.sort(rewinded)
for i = 1, math.max(#nonascii, #entries, #rewinded) do
  assert(nonascii[i] == entries[i])
  assert(entries[i] == rewinded[i])
end

-- Remove temporary workspace directory
assert(apr.filepath_set(cwd_save))
assert(apr.dir_remove_recursive 'io_dir_tests')

-- File I/O handling module (io_file.c) {{{1
message "Testing file I/O module ..\n"

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
local info = apr.stat(arg[0])
assert(type(info) == 'table')
assert(info.name == 'tests.lua')
assert(info.path:find 'tests%.lua$')
assert(info.type == 'file')
assert(info.size >= 1024 * 19)
assert(info.mtime >= 1293510503)
assert(info.nlink >= 1)
assert(type(info.inode) == 'number')
assert(type(info.dev) == 'number')
assert(info.protection:find '^[-r][-w][-xSs][-r][-w][-xSs][-r][-w][-xTt]$')

-- Test the alternative stat() interface. {{{2
assert(apr.stat(arg[0], 'type') == 'file')
assert(apr.stat(arg[0], 'name') == 'tests.lua')
local etcdir = apr.filepath_parent(arg[0])
local kind, size, prot = apr.stat(etcdir, 'type', 'size', 'protection')
assert(kind == 'directory')
assert(type(size) == 'number')
assert(prot:find '^[-r][-w][-xSs][-r][-w][-xSs][-r][-w][-xTt]$')

-- Test apr.file_perms_set().  {{{2
local tempname = assert(os.tmpname())
writefile(tempname, 'something')
local status, errmsg, errcode = apr.file_perms_set(tempname, 'rw-rw----')
if errcode ~= 'ENOTIMPL' then
  assert(apr.stat(tempname, 'protection') == 'rw-rw----')
  assert(apr.file_perms_set(tempname, 'ug=r,o='))
  assert(apr.stat(tempname, 'protection') == 'r--r-----')
end

-- Test apr.file_copy(). {{{2
local copy1 = assert(os.tmpname())
writefile(copy1, testdata)
local copy2 = assert(os.tmpname())
assert(apr.file_copy(copy1, copy2))
assert(testdata == readfile(copy2))

-- Test apr.file_append(). {{{2
assert(apr.file_append(copy1, copy2))
assert(readfile(copy2) == testdata:rep(2))

-- Test apr.file_rename(). {{{2
assert(apr.file_rename(copy1, copy2))
assert(not apr.stat(copy1))
assert(readfile(copy2) == testdata)

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
  message("Testing file I/O on a file of %i bytes (step %i/%i)..\n", #testdata, testsize, maxmultiplier)
  local testlines = {}
  for l in testdata:gmatch '[^\r\n]*' do
    table.insert(testlines, l)
  end

  -- Create temporary file with test data.
  local tempname = assert(os.tmpname())
  local testfile = assert(io.open(tempname, 'w'))
  assert(testfile:write(testdata))
  assert(testfile:close())

  -- Read back the temporary file's contents using various formats.
  local escapes = { ['\r'] = '\\r', ['\n'] = '\\n', ['"'] = '\\"', ['\0'] = '\\0' }
  local function formatvalue(v)
    if type(v) == 'number' then
      local s = string.format('%.99f', v)
      return s:find '%.' and (s:gsub('0+$', '0')) or s
    elseif type(v) == 'string' then
      return '"' .. v:gsub('[\r\n"%z]', escapes) .. '"'
      -- XXX return ('%q'):format(v)
    else
      return tostring(v)
    end
  end

  local function testformat(format)
    message("Testing file:read(%s) ..", format)
    local apr_file = assert(apr.file_open(tempname, 'r'))
    local lua_file = assert(io.open(tempname, 'r'))
    repeat
      local lua_value = lua_file:read(format)
      local apr_value = apr_file:read(format)
      if lua_value ~= apr_value then
        warning("Wrong result for file:read(%q)!\nLua value: %s\nAPR value: %s\n",
            format, formatvalue(lua_value), formatvalue(apr_value))
        warning("Lua position: %i, APR position: %i", lua_file:seek 'cur', apr_file:seek 'cur')
        warning("Remaining data in Lua file: %s", formatvalue(lua_file:read '*a' :sub(1, 50)))
        warning("Remaining data in APR file: %s", formatvalue(apr_file:read '*a' :sub(1, 50)))
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

  local lua_file = assert(os.tmpname())
  local apr_file = assert(os.tmpname())

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
      message("Testing file:write() mode %i, method %i ..", i, j)
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
            warning("Buggy output from file:write()?!\nLua value: %s\nAPR value: %s\n",
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

-- Miscellaneous routines (lua_apr.c) {{{1
message "Testing miscellaneous functions ..\n"

-- Test apr.type()
assert(apr.type(apr.file_open(arg[0])) == 'file')
assert(apr.type(apr.socket_create()) == 'socket')
assert(apr.type(apr.proc_create 'test') == 'process')
assert(apr.type(apr.dir_open '.') == 'directory')

-- Test apr.version_get()
local apr_v, apu_v = assert(apr.version_get())
assert(apr_v:find '^%d+%.%d+%.%d+$')
assert(apu_v:find '^%d+%.%d+%.%d+$')

-- Test status_to_name() (indirectly).
assert(select(3, apr.stat("I assume this won't exist")) == 'ENOENT')

-- Process handling module (proc.c, io_pipe.c) {{{1
message "Testing process handling module ..\n"

local function scriptpath(name)
  return assert(apr.filepath_merge(apr.filepath_parent(arg[0]), name))
end

local function newchild(cmdtype, script, env)
  local child = assert(apr.proc_create(arg[-1]))
  assert(child:cmdtype_set(cmdtype))
  if env then child:env_set(env) end
  assert(child:io_set('child-block', 'parent-block', 'parent-block'))
  assert(child:exec { scriptpath(script) })
  return child, assert(child:in_get()), assert(child:out_get()), assert(child:err_get())
end

-- Test bidirectional pipes. {{{2
local child, stdin, stdout, stderr = newchild('shellcmd/env', 'test-bidi-pipes.lua')
local testmsg = "Is There Anyone Out There?!"
assert(stdin:write(testmsg)); assert(stdin:close())
assert(stdout:read() == testmsg:lower())
assert(stderr:read() == testmsg:upper())

-- Test tostring(process). {{{2
assert(tostring(child):find '^process %([x%x]+%)$')

-- Test child environment manipulation. {{{2
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

-- Test apr.proc_fork() when supported. {{{2
if apr.proc_fork then
  local process, context = assert(apr.proc_fork())
  local forked_file, forked_text = assert(os.tmpname()), 'hello from forked child!'
  if context == 'child' then
    io.output(forked_file)
    io.write(forked_text, '\n')
    os.exit(0)
  elseif context == 'child' then
    for i = 1, 10 do if (apr.stat(forked_file, 'size') or 0) > 0 then break end apr.sleep(1) end
    assert(readfile(forked_file) == forked_text)
  end
end

-- Test apr.namedpipe_create(). {{{2

local namedpipe = os.tmpname()
local namedmsg = "Hello world over a named pipe!"
local status, errmsg, errcode = apr.namedpipe_create(namedpipe)
if errcode ~= 'ENOTIMPL' then
  local child = assert(apr.proc_create(arg[-1]))
  assert(child:cmdtype_set('shellcmd/env'))
  assert(child:exec { scriptpath 'test-namedpipe.lua', namedpipe, namedmsg })
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

-- Network I/O handling module (io_net.c) {{{1
message "Testing network I/O handling module ..\n"

-- Test apr.hostname_get(), apr.host_to_addr() and apr.addr_to_host().
local hostname = assert(apr.hostname_get())
local address = assert(apr.host_to_addr(hostname))
assert(apr.addr_to_host(address))

-- Test socket:bind(), socket:listen() and socket:accept().
local server = assert(apr.proc_create(arg[-1]))
local port = 12345
assert(server:cmdtype_set 'shellcmd/env')
assert(server:exec { scriptpath 'test-server.lua', port })
apr.sleep(5)
local client = assert(apr.socket_create())
assert(client:connect('localhost', port))
for _, msg in ipairs { 'First line', 'Second line', 'Third line' } do
  assert(client:write(msg, '\n'))
  assert(client:read() == msg:upper())
end
-- Test tostring(socket).
assert(tostring(client):find '^socket %([x%x]+%)$')
assert(client:close())
assert(tostring(client):find '^socket %(closed%)$')

-- String module (str.c) {{{1
message "Testing string module ..\n"

filenames = { 'rfc2086.txt', 'rfc1.txt', 'rfc822.txt' }
table.sort(filenames, apr.strnatcmp)
-- Test natural comparison.
assert(filenames[1] == 'rfc1.txt')
assert(filenames[2] == 'rfc822.txt')
assert(filenames[3] == 'rfc2086.txt')

filenames = { 'RFC2086.txt', 'RFC1.txt', 'rfc822.txt' }
table.sort(filenames, apr.strnatcasecmp)
-- Test case insensitive natural comparison.
assert(filenames[1] == 'RFC1.txt')
assert(filenames[2] == 'rfc822.txt')
assert(filenames[3] == 'RFC2086.txt')

-- Test binary size formatting.
assert(apr.strfsize(1024^1) == '1.0K')
assert(apr.strfsize(1024^2) == '1.0M')
assert(apr.strfsize(1024^3) == '1.0G')

command = [[ program argument1 "argument 2" 'argument 3' argument\ 4 ]]
cmdline = assert(apr.tokenize_to_argv(command))
-- Test command line tokenization.
assert(cmdline[1] == 'program')
assert(cmdline[2] == 'argument1')
assert(cmdline[3] == 'argument 2')
assert(cmdline[4] == 'argument 3')
assert(cmdline[5] == 'argument 4')

-- Time routines (time.c) {{{1
message "Testing time routines ..\n"

-- Based on http://svn.apache.org/viewvc/apr/apr/trunk/test/testtime.c?view=markup.

local now = 1032030336186711 / 1000000

-- Check that apr.time_now() more or less matches os.time()
assert(math.abs(os.time() - apr.time_now()) <= 1)

-- apr.time_explode() using apr_time_exp_lt()
local posix_exp = os.date('*t', now)
local xt = assert(apr.time_explode(now)) -- apr_time_exp_lt()
for k, v in pairs(posix_exp) do assert(v == xt[k]) end

-- apr.time_implode() on a local time table (ignores floating point precision
-- because on my laptop "now" equals 1032030336.186711 while "imp" equals
-- 1032037536.186710)
local imp = assert(apr.time_implode(xt)) -- apr_time_exp_gmt_get()
assert(math.floor(now) == math.floor(imp))

-- apr.time_implode() on a GMT time table
local xt = assert(apr.time_explode(now, true)) -- apr_time_exp_gmt()
local imp = assert(apr.time_implode(xt)) -- apr_time_exp_gmt_get()
assert(math.floor(now) == math.floor(imp))

-- Test apr.time_format() (example from http://en.wikipedia.org/wiki/Unix_time)
assert(apr.time_format('rfc822', 1000000000) == 'Sun, 09 Sep 2001 01:46:40 GMT')
assert(apr.time_format('%Y-%m', 1000000000) == '2001-09')

-- Test apr.sleep() :-)
local before = apr.time_now(); apr.sleep(1); local after = apr.time_now()
assert((after - before) >= 1)

-- URI parsing module (uri.c) {{{1
message "Testing URI parsing ..\n"

local hostinfo = 'scheme://user:pass@host:80'
local pathinfo = '/path/file?query-param=value#fragment'
local input = hostinfo .. pathinfo

-- Parse a URL into a table of components.
parsed = assert(apr.uri_parse(input))

-- Validate the parsed URL fields.
assert(parsed.scheme   == 'scheme')
assert(parsed.user     == 'user')
assert(parsed.password == 'pass')
assert(parsed.hostname == 'host')
assert(parsed.port     == '80')
assert(parsed.hostinfo == 'user:pass@host:80')
assert(parsed.path     == '/path/file')
assert(parsed.query    == 'query-param=value')
assert(parsed.fragment == 'fragment')

-- Check that complete and partial URL `unparsing' works.
assert(apr.uri_unparse(parsed) == input)
assert(apr.uri_unparse(parsed, 'hostinfo') == hostinfo)
assert(apr.uri_unparse(parsed, 'pathinfo') == pathinfo)

-- Make sure uri_port_of_scheme() works.
assert(apr.uri_port_of_scheme 'ssh' == 22)
assert(apr.uri_port_of_scheme 'http' == 80)
assert(apr.uri_port_of_scheme 'https' == 443)

-- The following string constant was generated using this PHP code:
--   $t=array();
--   for ($i=0; $i <= 255; $i++) $t[] = chr($i);
--   echo urlencode(join($t));
local uri_encoded = '%00%01%02%03%04%05%06%07%08%09%0A%0B%0C%0D%0E%0F%10%11%12%13%14%15%16%17%18%19%1A%1B%1C%1D%1E%1F+%21%22%23%24%25%26%27%28%29%2A%2B%2C-.%2F0123456789%3A%3B%3C%3D%3E%3F%40ABCDEFGHIJKLMNOPQRSTUVWXYZ%5B%5C%5D%5E_%60abcdefghijklmnopqrstuvwxyz%7B%7C%7D%7E%7F%80%81%82%83%84%85%86%87%88%89%8A%8B%8C%8D%8E%8F%90%91%92%93%94%95%96%97%98%99%9A%9B%9C%9D%9E%9F%A0%A1%A2%A3%A4%A5%A6%A7%A8%A9%AA%AB%AC%AD%AE%AF%B0%B1%B2%B3%B4%B5%B6%B7%B8%B9%BA%BB%BC%BD%BE%BF%C0%C1%C2%C3%C4%C5%C6%C7%C8%C9%CA%CB%CC%CD%CE%CF%D0%D1%D2%D3%D4%D5%D6%D7%D8%D9%DA%DB%DC%DD%DE%DF%E0%E1%E2%E3%E4%E5%E6%E7%E8%E9%EA%EB%EC%ED%EE%EF%F0%F1%F2%F3%F4%F5%F6%F7%F8%F9%FA%FB%FC%FD%FE%FF'
local chars = {}
for i = 0, 255 do chars[#chars+1] = string.char(i) end
local plain_chars = table.concat(chars)

-- Check that apr.uri_encode() works
assert(apr.uri_encode(plain_chars):lower() == uri_encoded:lower())

-- Check that apr.uri_decode() works
assert(apr.uri_decode(uri_encoded):lower() == plain_chars:lower())

-- User/group identification module (user.c) {{{1
message "Testing user/group identification ..\n"

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

-- Universally unique identifiers (uuid.c) {{{1
message "Testing universally unique identifiers ..\n"

-- Check that apr.uuid_get() returns at least 500 KB of unique strings.
local set = {}
assert(pcall(function()
  for i = 1, 32000 do
    uuid = assert(apr.uuid_get())
    assert(not set[uuid], 'duplicate UUID!')
    set[uuid] = true
  end
end))

-- I can't find any binary/formatted example UUIDs on the internet and don't
-- really know how to make the following tests useful. At least they illustrate
-- the purpose…

-- Check that apr.uuid_format() works.
assert(apr.uuid_format(('\0'):rep(16)) == '00000000-0000-0000-0000-000000000000')
assert(apr.uuid_format(('\255'):rep(16)) == 'ffffffff-ffff-ffff-ffff-ffffffffffff')

-- Check that apr.uuid_parse() works.
assert(apr.uuid_parse '00000000-0000-0000-0000-000000000000' == ('\0'):rep(16))
assert(apr.uuid_parse 'ffffffff-ffff-ffff-ffff-ffffffffffff' == ('\255'):rep(16))

-- }}}

-- vim: nowrap
