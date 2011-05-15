--[[

 This is the LuaRocks rockspec for the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: {{DATE}}
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

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
external_dependencies = {
  platforms = {
    unix = {
      APR = { header = 'apr-1.0/apr.h', library = 'apr-1' },
      APU = { header = 'apr-1.0/apu.h', library = 'aprutil-1' },
      APREQ = { header = 'apreq2/apreq.h', library = 'apreq2' },
    },
    macosx = {
      APR = { header = 'apr-1/apr.h', library = 'apr-1'},
      APU = { header = 'apr-1/apu.h', library = 'aprutil-1' },
      APREQ = { header = 'apreq2/apreq.h', library = 'apreq2' },
    },
  }
}
build = {
  type = 'make',
  variables = {
    LUA_DIR = '$(PREFIX)',
    LUA_LIBDIR = '$(LIBDIR)',
    LUA_SHAREDIR = '$(LUADIR)',
    CFLAGS = '$(CFLAGS) -I$(LUA_INCDIR) -I$(APR_INCDIR)/apr-1.0 -I$(APU_INCDIR)/apr-1.0 -I$(APREQ_INCDIR)/apreq2',
    LFLAGS = '$(LFLAGS) -L$(APR_LIBDIR) -L$(APU_LIBDIR) -L$(APREQ_LIBDIR) -lapr-1 -laprutil-1 -lapreq2',
  },
  platforms = {
    linux = {
      variables = {
        -- Make sure "apr_off_t" is defined correctly.
        CFLAGS = '$(CFLAGS) -I$(LUA_INCDIR) -I$(APR_INCDIR)/apr-1.0 -I$(APU_INCDIR)/apr-1.0 -I$(APREQ_INCDIR)/apreq2 -D_GNU_SOURCE',
      }
    },
    macosx = {
      variables = {
        -- Custom location for APR/APU headers on Mac OS X.
        -- FIXME I'm not sure whether _GNU_SOURCE needs to be defined.
        CFLAGS = '$(CFLAGS) -I$(LUA_INCDIR) -I$(APR_INCDIR)/apr-1 -I$(APU_INCDIR)/apr-1 -I$(APREQ_INCDIR)/apreq2 -D_GNU_SOURCE',
      }
    }
  }
}

-- vim: ft=lua ts=2 sw=2 et
