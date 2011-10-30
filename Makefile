# This is the UNIX makefile for the Lua/APR binding.
#
# Author: Peter Odding <peter@peterodding.com>
# Last Change: October 30, 2011
# Homepage: http://peterodding.com/code/lua/apr/
# License: MIT
#
# This makefile has been tested on Ubuntu Linux 10.04.

VERSION = $(shell grep _VERSION src/apr.lua | cut "-d'" -f2)
RELEASE = 1
PACKAGE = lua-apr-$(VERSION)-$(RELEASE)

# Based on http://www.luarocks.org/en/Recommended_practices_for_Makefiles
LUA_DIR = /usr/local
LUA_LIBDIR = $(LUA_DIR)/lib/lua/5.1
LUA_SHAREDIR = $(LUA_DIR)/share/lua/5.1

# Location for generated HTML documentation.
LUA_APR_DOCS = $(LUA_DIR)/share/doc/lua-apr

# Names of source / binary modules to install.
SOURCE_MODULE = src/apr.lua
BINARY_MODULE = core.so
APREQ_BINARY = apreq.so

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
		  src/http.c \
		  src/io_dir.c \
		  src/io_file.c \
		  src/io_net.c \
		  src/io_pipe.c \
		  src/ldap.c \
		  src/lua_apr.c \
		  src/memcache.c \
		  src/memory_pool.c \
		  src/object.c \
		  src/permissions.c \
		  src/proc.c \
		  src/shm.c \
		  src/signal.c \
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

# Determine compiler flags and linker flags for external dependencies using a
# combination of pkg-config, apr-1-config, apu-1-config and apreq2-config.
override CFLAGS += $(shell lua etc/make.lua --cflags)
override LFLAGS += $(shell lua etc/make.lua --lflags)

# Create debug builds by default but enable release
# builds using the command line "make DO_RELEASE=1".
ifndef DO_RELEASE
override CFLAGS += -g -DDEBUG
override LFLAGS += -g
endif

# Enable profiling with "make PROFILING=1".
ifdef PROFILING
override CFLAGS += -fprofile-arcs -ftest-coverage
override LFLAGS += -fprofile-arcs
endif

# Names of compiled object files.
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

# The default build rule checks for missing system packages before trying to
# build Lua/APR (only on supported platforms).
default: $(BINARY_MODULE)

# Build the binary module.
$(BINARY_MODULE): $(OBJECTS) Makefile
	$(CC) -shared -o $@ $(OBJECTS) $(LFLAGS) || lua etc/make.lua --check

# Build the standalone libapreq2 binding.
$(APREQ_BINARY): etc/apreq_standalone.c Makefile
	$(CC) -Wall -shared -o $@ $(CFLAGS) -fPIC etc/apreq_standalone.c $(LFLAGS) || lua etc/make.lua --check

# Compile individual source code files to object files.
$(OBJECTS): %.o: %.c src/lua_apr.h Makefile
	$(CC) -Wall -c $(CFLAGS) -fPIC $< -o $@ || lua etc/make.lua --check

# Always try to regenerate the error handling module.
src/errno.c: etc/errors.lua Makefile
	@lua etc/errors.lua

# Install the Lua/APR binding under $LUA_DIR.
install: $(BINARY_MODULE) docs
	mkdir -p $(LUA_SHAREDIR)/apr/test
	cp $(SOURCE_MODULE) $(LUA_SHAREDIR)/apr.lua
	cp test/*.lua $(LUA_SHAREDIR)/apr/test
	mkdir -p $(LUA_LIBDIR)/apr
	cp $(BINARY_MODULE) $(LUA_LIBDIR)/apr/$(BINARY_MODULE)
	[ ! -f $(APREQ_BINARY) ] || cp $(APREQ_BINARY) $(LUA_LIBDIR)/$(APREQ_BINARY)
	[ -d $(LUA_APR_DOCS) ] || mkdir -p $(LUA_APR_DOCS)
	cp doc/docs.html doc/notes.html doc/readme.html doc/todo.html $(LUA_APR_DOCS) 2>/dev/null || true

# Remove previously installed files.
uninstall:
	rm -f $(LUA_SHAREDIR)/apr.lua
	rm -fR $(LUA_SHAREDIR)/apr/test
	rm -fR $(LUA_LIBDIR)/apr
	cd $(LUA_APR_DOCS) && rm -r docs.html notes.html readme.html todo.html
	rmdir $(LUA_APR_DOCS) 2>/dev/null || true

# Run the test suite.
test: install
	export LD_PRELOAD=/lib/libSegFault.so; lua -e "require 'apr.test' ()"

# Run the test suite under Valgrind to detect and analyze errors.
valgrind:
	valgrind -q --track-origins=yes --leak-check=full lua -e "require 'apr.test' ()"

# Create or update test coverage report using "lcov".
coverage:
	[ -d etc/coverage ] || mkdir etc/coverage
	rm -f src/errno.gcda src/errno.gcno
	lcov -d src -b . --capture --output-file etc/coverage/lua-apr.info
	genhtml -o etc/coverage etc/coverage/lua-apr.info

# Convert the Markdown documents to HTML.
docs: doc/docs.md $(SOURCE_MODULE) $(SOURCES)
	@lua etc/wrap.lua doc/docs.md doc/docs.html
	@lua etc/wrap.lua README.md doc/readme.html
	@lua etc/wrap.lua NOTES.md doc/notes.html
	@lua etc/wrap.lua TODO.md doc/todo.html

# Extract the documentation from the source code and generate a Markdown file
# containing all documentation including coverage statistics (if available).
# XXX This file won't be regenerated automatically because A) the documentation
# in the ZIP archives I release contains coverage statistics that cannot be
# generated without first building and installing a profiling release and
# running the test suite and B) new users certainly won't know how to generate
# coverage statistics which means "make install" would overwrite the existing
# documentation and lose the coverage statistics...
doc/docs.md: etc/docs.lua
	@[ -d doc ] || mkdir doc
	@lua etc/docs.lua > doc/docs.md

# Create a profiling build, run the test suite, generate documentation
# including test coverage, then clean the intermediate files.
package_prerequisites: clean
	@echo Collecting coverage statistics using profiling build
	@export PROFILING=1; lua etc/buildbot.lua --local
	@echo Generating documentation including coverage statistics
	@rm -f doc/docs.md; make --no-print-directory docs

# Prepare a source ZIP archive from which Lua/APR can be build.
zip_package: package_prerequisites
	@rm -f $(PACKAGE).zip
	@mkdir -p $(PACKAGE)/doc
	@cp doc/docs.html doc/notes.html doc/readme.html doc/todo.html $(PACKAGE)/doc
	@mkdir -p $(PACKAGE)/etc
	@cp -a etc/buildbot.lua etc/make.lua etc/docs.lua etc/errors.lua etc/wrap.lua $(PACKAGE)/etc
	@mkdir -p $(PACKAGE)/benchmarks
	@cp -a benchmarks/* $(PACKAGE)/benchmarks
	@mkdir -p $(PACKAGE)/examples
	@cp -a examples/*.lua $(PACKAGE)/examples
	@mkdir -p $(PACKAGE)/src
	@cp -a src/lua_apr.h $(SOURCES) $(SOURCE_MODULE) $(PACKAGE)/src
	@mkdir -p $(PACKAGE)/test
	@cp -a test/*.lua $(PACKAGE)/test
	@cp Makefile Makefile.win make.cmd README.md NOTES.md TODO.md $(PACKAGE)
	@zip $(PACKAGE).zip -qr $(PACKAGE)
	@echo Generated $(PACKAGE).zip
	@rm -R $(PACKAGE)

# Prepare a LuaRocks rockspec for the current release.
rockspec: zip_package
	@cat etc/template.rockspec \
		| sed "s/{{VERSION}}/$(VERSION)-$(RELEASE)/g" \
		| sed "s/{{DATE}}/`export LANG=; date '+%B %d, %Y'`/" \
		| sed "s/{{HASH}}/`md5sum $(PACKAGE).zip | cut '-d ' -f1 `/" \
		> lua-apr-$(VERSION)-$(RELEASE).rockspec
	@echo Generated $(PACKAGE).rockspec

# Clean generated files from working directory.
clean:
	@rm -Rf $(OBJECTS) $(BINARY_MODULE) $(APREQ_BINARY) doc/docs.md
	@git checkout src/errno.c 2>/dev/null || true

.PHONY: install uninstall test valgrind coverage docs \
	package_prerequisites zip_package rockspec clean

# vim: ts=4 sw=4 noet
