--[[

 Unit tests for the directory manipulation module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: November 27, 2011
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
local directory = assert(apr.temp_dir_get())
assert(helpers.writable(directory))

-- Create a temporary workspace directory for the following tests
math.randomseed(os.time())
local io_dir_tests = apr.filepath_merge(directory,
    string.format('lua-apr-io_dir_tests-%i', math.random(1e9)))
assert(apr.dir_make(io_dir_tests))

-- Change to the temporary workspace directory
local cwd_save = assert(apr.filepath_get())
assert(apr.filepath_set(io_dir_tests))

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
local msi = 'Mindless Self Indulgence - Despierta Los Niños'
local manu_chao = 'Manu Chao - Próxima Estación; Esperanza'
local cassius = 'Cassius - Au Rêve'
local yoko_kanno = '菅野 よう子'
local nonascii = { msi, manu_chao, cassius, yoko_kanno }

-- Workarounds for Unicode normalization on Mac OS X. {{{1

-- Mac OS X apparently applies Unicode normalization on file system level. This
-- means you can write a file without getting any errors, but when you check
-- for the file later it doesn't exist (because the normalized form differs
-- from the original form and the normalized form is reported back by the OS).
-- See also issue 10 on GitHub: https://github.com/xolox/lua-apr/issues/10.
-- The following table maps the titles above to their normalized form.
local nonascii_normalized = {
  -- Technically it's superstition that made me write out these normalized
  -- forms as sequences of bytes. I did so because the visual differences
  -- between the normalized forms and the original track titles above are so
  -- small it hurts my head :-(
  [string.char(77, 105, 110, 100, 108, 101, 115, 115, 32, 83, 101, 108, 102, 32, 73, 110, 100, 117, 108, 103, 101, 110, 99, 101, 32, 45, 32, 68, 101, 115, 112, 105, 101, 114, 116, 97, 32, 76, 111, 115, 32, 78, 105, 110, 204, 131, 111, 115)] = msi,
  [string.char(77, 97, 110, 117, 32, 67, 104, 97, 111, 32, 45, 32, 80, 114, 111, 204, 129, 120, 105, 109, 97, 32, 69, 115, 116, 97, 99, 105, 111, 204, 129, 110, 59, 32, 69, 115, 112, 101, 114, 97, 110, 122, 97)] = manu_chao,
  [string.char(67, 97, 115, 115, 105, 117, 115, 32, 45, 32, 65, 117, 32, 82, 101, 204, 130, 118, 101)] = cassius,
}

local warned_about_normalization = false

local function handle_normalized_forms(s)
  if nonascii_normalized[s] then
    if not warned_about_normalization then
      helpers.warning "Detected Unicode normalization on file system level, applying workaround... (you're probably on Mac OS X)\n"
      warned_about_normalization = true
    end
    return nonascii_normalized[s]
  else
    return s
  end
end

-- }}}1

for _, name in ipairs(nonascii) do
  assert(apr.dir_make(name))
  -- Using writable() here won't work because the APR API deals with UTF-8
  -- while the Lua API does not, which makes the strings non-equal... :-)
  assert(assert(apr.stat(name, 'type')) == 'directory')
end

-- Test apr.dir_open() and directory methods.
local dir = assert(apr.dir_open '.')
local entries = {}
for name in dir:entries 'name' do entries[#entries+1] = handle_normalized_forms(name) end
assert(dir:rewind())
local rewinded = {}
for name in dir:entries 'name' do rewinded[#rewinded+1] = handle_normalized_forms(name) end
assert(tostring(dir):find '^directory %([x%x]+%)$')
assert(dir:close())
assert(tostring(dir):find '^directory %(closed%)$')
table.sort(nonascii)
table.sort(entries)
table.sort(rewinded)
if not pcall(function()
  for i = 1, math.max(#nonascii, #entries, #rewinded) do
    assert(nonascii[i] == entries[i])
    assert(entries[i] == rewinded[i])
  end
end) then
  error("Directory traversal returned incorrect result?\n"
    .. 'Input strings:     "' .. table.concat(nonascii, '", "') .. '"\n'
    .. 'Directory entries: "' .. table.concat(entries, '", "') .. '"\n'
    .. 'Rewinded entries:  "' .. table.concat(rewinded, '", "') .. '"')
end

-- Remove temporary workspace directory
assert(apr.filepath_set(cwd_save))
assert(apr.dir_remove_recursive(io_dir_tests))
