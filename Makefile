# This is the UNIX makefile for the Lua/APR binding.
#
# Author: Peter Odding <peter@peterodding.com>
# Last Change: February 20, 2011
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

# Names of source code files to compile & link (the individual lines enable
# automatic rebasing between git feature branches and the master branch).
SOURCES = src/base64.c \
		  src/buffer.c \
		  src/crypt.c \
		  src/date.c \
		  src/dbd.c \
		  src/dbm.c \
		  src/env.c \
		  src/errno.c \
		  src/filepath.c \
		  src/fnmatch.c \
		  src/getopt.c \
		  src/io_dir.c \
		  src/io_file.c \
		  src/io_net.c \
		  src/io_pipe.c \
		  src/lua_apr.c \
		  src/memory_pool.c \
		  src/object.c \
		  src/permissions.c \
		  src/proc.c \
		  src/shm.c \
		  src/stat.c \
		  src/str.c \
		  src/thread.c \
		  src/thread_queue.c \
		  src/time.c \
		  src/tuple.c \
		  src/uri.c \
		  src/user.c \
		  src/uuid.c \
		  src/xlate.c \
		  src/xml.c

# Names of compiled object files.
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

# If you're building Lua/APR with LuaRocks it should locate the external
# dependencies automatically, otherwise we fall back to `pkg-config'. On
# Debian/Ubuntu the Lua pkg-config file is called "lua5.1", on Arch Linux its
# just "lua".
CFLAGS = $(shell pkg-config --cflags lua5.1 --silence-errors || pkg-config --cflags lua) \
		 $(shell pkg-config --cflags apr-1) \
		 $(shell pkg-config --cflags apr-util-1)
LFLAGS = $(shell pkg-config --libs apr-1) \
		 $(shell pkg-config --libs apr-util-1)

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

$(BINARY_MODULE): $(OBJECTS) Makefile
	$(CC) -shared -o $(BINARY_MODULE) $(OBJECTS) $(LFLAGS)

$(OBJECTS): %.o: %.c src/lua_apr.h Makefile
	$(CC) -Wall -c $(CFLAGS) -fPIC $< -o $@

src/errno.c: etc/errors.lua Makefile
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

# FIXME The libreadline-dev isn't really needed here is it?!
install_deps:
	apt-get install libapr1 libapr1-dev \
		libaprutil1 libaprutil1-dev libaprutil1-dbd-sqlite3 \
		lua5.1 liblua5.1-0 liblua5.1-0-dev libreadline-dev \
		liblua5.1-markdown0

ZIPNAME = lua-apr-0.14.2-1

package: docs
	@echo Packaging sources
	@rm -f $(ZIPNAME).zip
	@mkdir -p $(ZIPNAME)/doc
	@cp docs.html $(ZIPNAME)/doc/apr.html
	@mkdir -p $(ZIPNAME)/etc
	@cp -a etc/docs.lua etc/errors.lua $(ZIPNAME)/etc
	@mkdir -p $(ZIPNAME)/benchmarks
	@cp -a benchmarks/* $(ZIPNAME)/benchmarks
	@mkdir -p $(ZIPNAME)/examples
	@cp -a examples/*.lua $(ZIPNAME)/examples
	@mkdir -p $(ZIPNAME)/src
	@cp -a src/lua_apr.h $(SOURCES) $(SOURCE_MODULE) $(ZIPNAME)/src
	@mkdir -p $(ZIPNAME)/test
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
