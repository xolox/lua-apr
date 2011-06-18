--[[

 This is the LuaRocks rockspec for the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: June 18, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

package = 'Lua-APR'
version = 'scm-1'
source = { url = 'git://github.com/xolox/lua-apr.git', branch = 'master' }
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
