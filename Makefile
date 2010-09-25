# This is the UNIX makefile for the Lua/APR binding.
#
# Author: Peter Odding <peter@peterodding.com>
# Last Change: September 25, 2010
# Homepage: http://peterodding.com/code/lua/apr/
# License: MIT
#
# This makefile has been tested on Ubuntu Linux 10.04
# after running the `install_deps' target below.

# Names of source / binary modules to install.
SOURCE_MODULE = src/apr.lua
BINARY_MODULE = core.so

# Names of source code files to compile & link.
SOURCES = src/base64.c src/crypt.c src/env.c src/filepath.c src/fnmatch.c \
          src/io_dir.c src/lua_apr.c src/permissions.c src/stat.c src/str.c \
          src/time.c src/uri.c src/user.c src/uuid.c src/io_file.c

# Names of compiled object files.
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

# Use `pkg-config' to get the required compile/link arguments.
CFLAGS := $(shell pkg-config --cflags lua5.1) \
          $(shell pkg-config --cflags apr-1) \
          $(shell pkg-config --cflags apr-util-1)
LFLAGS := $(shell pkg-config --libs lua5.1) \
          $(shell pkg-config --libs apr-1) \
          $(shell pkg-config --libs apr-util-1)

# The rules.

$(BINARY_MODULE): $(OBJECTS)
	$(CC) -shared -g -o $(BINARY_MODULE) $(OBJECTS) $(LFLAGS) -lpthread

# NB: Without -lpthread, GDB errors out with "Cannot find new threads: generic error".

$(OBJECTS): %.o: %.c
	$(CC) -Wall -g -c $(CFLAGS) -fPIC $< -o $@

install: $(BINARY_MODULE)
	@sudo mkdir -p /usr/local/share/lua/5.1
	sudo cp $(SOURCE_MODULE) /usr/local/share/lua/5.1/apr.lua
	@sudo mkdir -p /usr/local/lib/lua/5.1/apr
	sudo cp $(BINARY_MODULE) /usr/local/lib/lua/5.1/apr/core.so

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

.PHONY: clean install test docs install_deps

# vim: ts=4 sw=4 noet
