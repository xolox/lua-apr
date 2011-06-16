--[[

 Unit tests for the buffered I/O interface of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: June 15, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

local helpers = require 'apr.test.helpers'
local verbosity = 0

local function testformat(apr_file, lua_file, format)
  if verbosity >= 1 then helpers.message("Testing file:read(%s) ..\n", format) end
  repeat
    local lua_value = lua_file:read(format)
    if verbosity >= 2 then helpers.message("file:read(%s) = %s\n", format, helpers.formatvalue(lua_value)) end
    local apr_value = apr_file:read(format)
    if lua_value ~= apr_value then
      helpers.warning("Wrong result for file:read(%s)!\nLua value: %s\nAPR value: %s\n",
          format, helpers.formatvalue(lua_value), helpers.formatvalue(apr_value))
      helpers.warning("Lua position: %i, APR position: %i\n", lua_file:seek 'cur', apr_file:seek 'cur')
      helpers.warning("Remaining data in Lua file: %s\n", helpers.formatvalue(lua_file:read '*a'))
      helpers.warning("Remaining data in APR file: %s\n", helpers.formatvalue(apr_file:read '*a'))
      os.exit(1)
    end
  until (format == '*a' and lua_value == '') or not lua_value
end

return function(test_file, apr_object)
  local lua_file = assert(io.open(test_file))
  for _, format in pairs { '*n', '*l', '*a', 1, 2, 3, 4, 5, 10, 20, 50, 100 } do
    testformat(apr_object, lua_file, format)
    apr_object:seek('set', 0)
    lua_file:seek('set', 0)
  end
end
