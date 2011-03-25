--[[

 Unit tests for the file path manipulation module of the Lua/APR binding.

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
local helpers = require 'apr.test.helpers'

-- Test apr.filepath_root(). {{{1
if apr.platform_get() == 'WIN32' then
  assert(apr.filepath_root 'c:/foo/bar' == 'c:/')
else
  -- Doesn't really do anything useful on UNIX :-P
  assert(apr.filepath_root '/foo/bar' == '/')
end

-- Test apr.filepath_parent(). {{{1

for _, path in ipairs {
  'foo/bar/baz', -- clean and simple path with 3 segments
  'foo//bar//baz///' -- same path with some empty segments (should be ignored)
} do
  local root = apr.platform_get() == 'WIN32' and 'c:/' or '/'
  local abs = function(s) return apr.filepath_merge(root, s) end
  local p1, n1 = apr.filepath_parent(abs(path))
  local p2, n2 = apr.filepath_parent(p1)
  local p3, n3 = apr.filepath_parent(p2)
  local p4, n4 = apr.filepath_parent(p3)
  assert(p4 == root and n4 == '')
  assert(p3 == root and n3 == 'foo')
  assert(p2 == abs 'foo/' and n2 == 'bar')
  assert(p1 == abs 'foo/bar/' and n1 == 'baz')
end

-- Test apr.filepath_name(). {{{1
assert(apr.filepath_name('/usr/bin/lua') == 'lua')
local parts = { apr.filepath_name('/home/xolox/.vimrc', true) }
assert(#parts == 2 and parts[1] == '.vimrc' and parts[2] == '')
parts = { apr.filepath_name('index.html.en', true) }
assert(#parts == 2 and parts[1] == 'index.html' and parts[2] == '.en')

-- Test apr.filepath_merge(). {{{1
if apr.platform_get() == 'WIN32' then
  assert(apr.filepath_merge('c:/', 'foo') == 'c:/foo')
else
  assert(apr.filepath_merge('/foo', 'bar') == '/foo/bar')
end

-- Test apr.filepath_list_split()/merge(). {{{1
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
