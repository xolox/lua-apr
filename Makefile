# This is the UNIX makefile for the Lua/APR binding.
#
# Author: Peter Odding <peter@peterodding.com>
# Last Change: February 12, 2011
# Homepage: http://peterodding.com/code/lua/apr/
# License: MIT
#
# This makefile has been tested on Ubuntu Linux 10.04 after installing the
# external dependencies using the `install_deps' target (see below).

# Based on http://www.luarocks.org/en/Recommended_practices_for_Makefiles
LUA_DIR = /usr/local
LUA_LIBDIR = $(LUA_DIR)/lib/lua/5.1
LUA_SHAREDIR = $(LUA_DIR)/share/lua/5.1

# Names of source / binary modules to install.
SOURCE_MODULE = src/apr.lua
BINARY_MODULE = core.so

# Names of source code files to compile & link.
SOURCES = src/base64.c src/buffer.c src/crypt.c src/date.c src/dbd.c src/dbm.c \
		  src/env.c src/errno.c src/filepath.c src/fnmatch.c src/io_dir.c \
		  src/io_file.c src/io_net.c src/io_pipe.c src/lua_apr.c \
		  src/permissions.c src/proc.c src/refpool.c src/stat.c src/shm.c \
		  src/str.c src/thread.c src/time.c src/uri.c src/user.c src/uuid.c \
		  src/xlate.c src/xml.c

# Names of compiled object files.
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

# If you're building Lua/APR with LuaRocks it should locate the external
# dependencies automatically, otherwise we fall back to `pkg-config'.
CFLAGS = `pkg-config --cflags lua5.1` `pkg-config --cflags apr-1` `pkg-config --cflags apr-util-1`
LFLAGS = `pkg-config --libs apr-1` `pkg-config --libs apr-util-1`

# Create debug builds by default but enable release
# builds using the command line "make RELEASE=1".
ifndef RELEASE
CFLAGS := $(CFLAGS) -g -DDEBUG
LFLAGS := $(LFLAGS) -g
endif

# Enable profiling with "make PROFILING=1".
ifdef PROFILING
CFLAGS := $(CFLAGS) -fprofile-arcs -ftest-coverage
LFLAGS := $(LFLAGS) -fprofile-arcs
endif

# The build rules.

$(BINARY_MODULE): $(OBJECTS)
	$(CC) -shared -o $(BINARY_MODULE) $(OBJECTS) $(LFLAGS)

$(OBJECTS): %.o: %.c src/lua_apr.h
	$(CC) -Wall -c $(CFLAGS) -fPIC $< -o $@

src/errno.c: etc/errors.lua
	lua etc/errors.lua > src/errno.c.new && mv src/errno.c.new src/errno.c

install: $(BINARY_MODULE)
	mkdir -p $(LUA_SHAREDIR)/apr/test
	cp $(SOURCE_MODULE) $(LUA_SHAREDIR)/apr.lua
	cp test/*.lua $(LUA_SHAREDIR)/apr/test
	mkdir -p $(LUA_LIBDIR)/apr
	cp $(BINARY_MODULE) $(LUA_LIBDIR)/apr/$(BINARY_MODULE)

uninstall:
	rm -f $(LUA_SHAREDIR)/apr.lua
	rm -fR $(LUA_SHAREDIR)/apr/test
	rm -fR $(LUA_LIBDIR)/apr

test:
	lua -lapr.test

valgrind:
	valgrind -q --track-origins=yes --leak-check=full lua -lapr.test

coverage:
	[ -d etc/coverage ] || mkdir etc/coverage
	rm -f src/errno.gcda src/errno.gcno
	lcov -d src -b . --capture --output-file etc/coverage/lua-apr.info
	genhtml -o etc/coverage etc/coverage/lua-apr.info

docs: etc/docs.lua $(SOURCE_MODULE) $(SOURCES)
	@echo Generating documentation..
	@lua etc/docs.lua docs.md docs.html

install_deps:
	apt-get install libapr1 libapr1-dev libaprutil1 libaprutil1-dev \
		lua5.1 liblua5.1-0 liblua5.1-0-dev libreadline-dev \
		liblua5.1-markdown0

ZIPNAME = lua-apr-0.11-1

package: docs
	@echo Packaging sources
	@rm -f $(ZIPNAME).zip
	@mkdir -p $(ZIPNAME)/doc $(ZIPNAME)/etc $(ZIPNAME)/examples $(ZIPNAME)/src  $(ZIPNAME)/test
	@cp docs.html $(ZIPNAME)/doc/apr.html
	@cp -a etc/docs.lua etc/errors.lua $(ZIPNAME)/etc
	@cp -a examples/download.lua examples/webserver.lua $(ZIPNAME)/examples
	@cp -a src/lua_apr.h $(SOURCES) $(SOURCE_MODULE) $(ZIPNAME)/src
	@cp -a test/*.lua $(ZIPNAME)/test
	@cp Makefile Makefile.win make.cmd NOTES.md README.md $(ZIPNAME)
	@zip $(ZIPNAME).zip -r $(ZIPNAME)
	@rm -R $(ZIPNAME)
	@echo Calculating MD5 sum for LuaRocks
	@md5sum $(ZIPNAME).zip

clean:
	@rm -Rf $(BINARY_MODULE) $(OBJECTS) etc/coverage
	@which lcov && lcov -z -d .
	@rm -f src/*.gcov src/*.gcno

.PHONY: install uninstall test valgrind coverage docs install_deps package clean

# vim: ts=4 sw=4 noet
