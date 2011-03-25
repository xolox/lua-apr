--[[

 Unit tests for the directory manipulation module of the Lua/APR binding.

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

-- Make sure apr.temp_dir_get() returns an existing, writable directory
assert(helpers.writable(assert(apr.temp_dir_get())))

-- Create a temporary workspace directory for the following tests
assert(apr.dir_make 'io_dir_tests')

-- Change to the temporary workspace directory
local cwd_save = assert(apr.filepath_get())
assert(apr.filepath_set 'io_dir_tests')

-- Test dir_make()
assert(apr.dir_make 'foobar')
assert(helpers.writable 'foobar')

-- Test dir_remove()
assert(apr.dir_remove 'foobar')
assert(not helpers.writable 'foobar')

-- Test dir_make_recursive() and dir_remove_recursive()
assert(apr.dir_make_recursive 'foo/bar/baz')
assert(helpers.writable 'foo/bar/baz')
assert(apr.dir_remove_recursive 'foo')
assert(not helpers.writable 'foo')

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
