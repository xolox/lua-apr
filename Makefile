# This is the UNIX makefile for the Lua/APR binding.
#
# Author: Peter Odding <peter@peterodding.com>
# Last Change: October 25, 2010
# Homepage: http://peterodding.com/code/lua/apr/
# License: MIT
#
# This makefile has been tested on Ubuntu Linux 10.04 after installing the
# external dependencies using the `install_deps' target (see below).

# Based on http://www.luarocks.org/en/Recommended_practices_for_Makefiles
LUA_DIR=/usr/local
LUA_LIBDIR=$(LUA_DIR)/lib/lua/5.1
LUA_SHAREDIR=$(LUA_DIR)/share/lua/5.1

# Names of source / binary modules to install.
SOURCE_MODULE = src/apr.lua
BINARY_MODULE = core.so

# Names of source code files to compile & link.
SOURCES = src/base64.c src/buffer.c src/crypt.c src/dbm.c src/env.c \
		  src/filepath.c src/fnmatch.c src/io_dir.c src/io_file.c \
		  src/io_pipe.c src/lua_apr.c src/permissions.c src/proc.c \
		  src/refpool.c src/stat.c src/str.c src/time.c src/uri.c \
		  src/user.c src/uuid.c

# Names of compiled object files.
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

# If you're building Lua/APR with LuaRocks it should locate the external
# dependencies automatically, otherwise we fall back to `pkg-config'.
CFLAGS = `pkg-config --cflags lua5.1` `pkg-config --cflags apr-1` `pkg-config --cflags apr-util-1`
LFLAGS = `pkg-config --libs lua5.1` `pkg-config --libs apr-1` `pkg-config --libs apr-util-1`

# Create debug builds by default but enable release
# builds using the command line "make RELEASE=1".
ifndef RELEASE
CFLAGS := $(CFLAGS) -g -DDEBUG
LFLAGS := $(LFLAGS) -g
endif

# The build rules.

$(BINARY_MODULE): $(OBJECTS)
	$(CC) -shared -o $(BINARY_MODULE) $(OBJECTS) $(LFLAGS)

$(OBJECTS): %.o: %.c src/lua_apr.h
	$(CC) -Wall -c $(CFLAGS) -fPIC $< -o $@

install: $(BINARY_MODULE)
	@mkdir -p $(LUA_SHAREDIR)
	cp $(SOURCE_MODULE) $(LUA_SHAREDIR)/apr.lua
	@mkdir -p $(LUA_LIBDIR)/apr
	cp $(BINARY_MODULE) $(LUA_LIBDIR)/apr/core.so

uninstall:
	@rm $(LUA_SHAREDIR)/apr.lua
	@rm $(LUA_LIBDIR)/apr/core.so
	@rmdir $(LUA_LIBDIR)/apr

test:
	@which shake >/dev/null && shake etc/tests.lua || lua etc/tests.lua

docs: etc/docs.lua src/apr.lua $(SOURCES)
	@echo Generating documentation..
	@lua etc/docs.lua docs.md docs.html

install_deps:
	apt-get install libapr1 libapr1-dev libaprutil1 libaprutil1-dev \
		lua5.1 liblua5.1-0 liblua5.1-0-dev libreadline-dev shake \
		liblua5.1-markdown0

ZIPNAME = lua-apr-0.9.3-1

package: docs
	@echo Packaging sources
	@rm -f $(ZIPNAME).zip
	@mkdir -p $(ZIPNAME)/doc $(ZIPNAME)/etc $(ZIPNAME)/src
	@cp -a src/lua_apr.h $(SOURCES) src/apr.lua $(ZIPNAME)/src
	@cp -a etc/docs.lua etc/tests.lua \
		etc/lua-apr-0.6-1.rockspec \
		etc/lua-apr-0.6-3.rockspec \
		etc/lua-apr-scm-1.rockspec \
		etc/lua-apr-0.7-2.rockspec \
		$(ZIPNAME)/etc
	@cp Makefile Makefile.win NOTES.md README.md $(ZIPNAME)
	@cp docs.html $(ZIPNAME)/doc/apr.html
	@zip $(ZIPNAME).zip -r $(ZIPNAME)
	@rm -R $(ZIPNAME)
	@echo Calculating MD5 sum for LuaRocks
	@md5sum $(ZIPNAME).zip

clean:
	@rm -f $(BINARY_MODULE) $(OBJECTS)

.PHONY: install uninstall test docs install_deps clean

# vim: ts=4 sw=4 noet
