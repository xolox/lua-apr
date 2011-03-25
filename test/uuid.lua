--[[

 Unit tests for the universally unique identifiers module of the Lua/APR binding.

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
