# This is the UNIX makefile for the Lua/APR binding.
#
# Author: Peter Odding <peter@peterodding.com>
# Last Change: September 26, 2010
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
SOURCES = src/base64.c src/crypt.c src/env.c src/filepath.c src/fnmatch.c \
          src/io_dir.c src/lua_apr.c src/permissions.c src/stat.c src/str.c \
          src/time.c src/uri.c src/user.c src/uuid.c src/io_file.c

# Names of compiled object files.
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

# If you're building Lua/APR with LuaRocks it should locate the external
# dependencies automatically, otherwise we fall back to `pkg-config'.
CFLAGS = `pkg-config --cflags lua5.1` `pkg-config --cflags apr-1` `pkg-config --cflags apr-util-1`
LFLAGS = `pkg-config --libs lua5.1` `pkg-config --libs apr-1` `pkg-config --libs apr-util-1`

# The build rules.

$(BINARY_MODULE): $(OBJECTS)
	$(CC) -shared -o $(BINARY_MODULE) $(OBJECTS) $(LFLAGS)

$(OBJECTS): %.o: %.c
	$(CC) -Wall -g -c $(CFLAGS) -fPIC $< -o $@

install: $(BINARY_MODULE)
	@mkdir -p $(LUA_SHAREDIR)
	cp $(SOURCE_MODULE) $(LUA_SHAREDIR)/apr.lua
	@mkdir -p $(LUA_LIBDIR)/apr
	cp $(BINARY_MODULE) $(LUA_LIBDIR)/apr/core.so

uninstall:
	@rm -Rf $(LUA_LIBDIR)/apr/ $(LUA_SHAREDIR)/apr.lua

test: install
	@which shake >/dev/null && shake etc/tests.lua || lua etc/tests.lua

docs: etc/docs.lua src/apr.lua $(SOURCES)
	@lua etc/docs.lua > docs.html

install_deps:
	sudo apt-get install libapr1 libapr1-dev libaprutil1 libaprutil1-dev \
		lua5.1 liblua5.1-0 liblua5.1-0-dev libreadline-dev shake \
		liblua5.1-markdown0

clean:
	@rm -f $(BINARY_MODULE) $(OBJECTS) docs.html

.PHONY: install uninstall test docs install_deps clean

# vim: ts=4 sw=4 noet
