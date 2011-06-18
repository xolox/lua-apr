--[[

 This is the LuaRocks rockspec for the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: {{DATE}}
 Homepage: http://peterodding.com/code/lua/apr/

--]]

package = 'Lua-APR'
version = '{{VERSION}}'
source = {
  url = 'http://peterodding.com/code/lua/apr/downloads/lua-apr-{{VERSION}}.zip',
  md5 = '{{HASH}}',
}
description = {
  summary = 'Apache Portable Runtime binding for Lua',
  detailed = [[
    Lua/APR is a binding to the Apache Portable Runtime (APR) library. APR
    powers software such as the Apache webserver and Subversion and Lua/APR
    makes the APR operating system interfaces available to Lua.
  ]],
  homepage = 'http://peterodding.com/code/lua/apr/',
  license = 'MIT',
}
dependencies = { 'lua >= 5.1' }
build = {
  type = 'make',
  variables = {
    LUA_DIR = '$(PREFIX)',
    LUA_LIBDIR = '$(LIBDIR)',
    LUA_SHAREDIR = '$(LUADIR)',
    CFLAGS = '$(CFLAGS) -I$(LUA_INCDIR)',
  }
}

-- vim: ft=lua ts=2 sw=2 et
