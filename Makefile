# This is the UNIX makefile for the Lua/APR binding.
#
# Author: Peter Odding <peter@peterodding.com>
# Last Change: June 19, 2011
# Homepage: http://peterodding.com/code/lua/apr/
# License: MIT
#
# This makefile has been tested on Ubuntu Linux 10.04 after installing the
# external dependencies using the `install_deps' target (see below).

VERSION = $(shell grep _VERSION src/apr.lua | cut "-d'" -f2)
RELEASE = $(shell grep _RELEASE src/apr.lua | cut "-d'" -f2)
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
		  src/io_dir.c \
		  src/io_file.c \
		  src/io_net.c \
		  src/io_pipe.c \
		  src/lua_apr.c \
		  src/memcache.c \
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

# If you're building Lua/APR with LuaRocks it should locate the external
# dependencies automatically, otherwise we fall back to `pkg-config'. Some
# complicating factors: On Debian/Ubuntu the Lua pkg-config file is called
# `lua5.1', on FreeBSD it's `lua-5.1' and on Arch Linux it's just `lua'. Also
# `pkg-config --cflags apr-1' doesn't include -pthread while `apr-1-config
# --cflags' does include this flag and this seems to be needed on some
# platforms. See also issue #5 on GitHub: https://github.com/xolox/lua-apr/issues/5
override CFLAGS += \
 $(shell pkg-config --cflags lua5.1 --silence-errors || pkg-config --cflags lua-5.1 --silence-errors || pkg-config --cflags lua) \
 $(shell apr-1-config --cflags --cppflags --includes 2>/dev/null || pkg-config --cflags apr-1) \
 $(shell apu-1-config --includes 2>/dev/null || pkg-config --cflags apr-util-1)
override LFLAGS += \
 $(shell apr-1-config --link-ld --libs 2>/dev/null || pkg-config --libs apr-1) \
 $(shell apu-1-config --link-ld --libs 2>/dev/null || pkg-config --libs apr-util-1)

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

# Experimental support for HTTP request parsing using libapreq2.
ifndef DISABLE_APREQ
override SOURCES += src/http.c
override CFLAGS += $(shell apreq2-config --includes)
override LFLAGS += $(shell apreq2-config --link-ld)
else
override CFLAGS += -DLUA_APR_DISABLE_APREQ
endif

# Names of compiled object files.
OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

# Build the binary module.
$(BINARY_MODULE): $(OBJECTS) Makefile
	$(CC) -shared -o $@ $(OBJECTS) $(LFLAGS)

# Build the standalone libapreq2 binding.
$(APREQ_BINARY): etc/apreq_standalone.c Makefile
	$(CC) -Wall -shared -o $@ $(CFLAGS) -fPIC etc/apreq_standalone.c $(LFLAGS)

# Compile individual source code files to object files.
$(OBJECTS): %.o: %.c src/lua_apr.h Makefile
	$(CC) -Wall -c $(CFLAGS) -fPIC $< -o $@

# Always try to regenerate the error handling module.
src/errno.c: etc/errors.lua Makefile
	@lua etc/errors.lua > src/errno.c.new && mv -f src/errno.c.new src/errno.c || true

# Install the Lua/APR binding under $LUA_DIR.
install: $(BINARY_MODULE)
	mkdir -p $(LUA_SHAREDIR)/apr/test
	cp $(SOURCE_MODULE) $(LUA_SHAREDIR)/apr.lua
	cp test/*.lua $(LUA_SHAREDIR)/apr/test
	mkdir -p $(LUA_LIBDIR)/apr
	cp $(BINARY_MODULE) $(LUA_LIBDIR)/apr/$(BINARY_MODULE)
	if [ -e $(APREQ_BINARY) ]; then cp $(APREQ_BINARY) $(LUA_LIBDIR)/$(APREQ_BINARY); fi
	make --no-print-directory docs
	if [ ! -d $(LUA_APR_DOCS) ]; then mkdir -p $(LUA_APR_DOCS); fi
	cp docs.html $(LUA_APR_DOCS)/
	lua etc/wrap.lua < README.md > $(LUA_APR_DOCS)/readme.html
	lua etc/wrap.lua < NOTES.md > $(LUA_APR_DOCS)/notes.html
	lua etc/wrap.lua < TODO.md > $(LUA_APR_DOCS)/todo.html

# Remove previously installed files.
uninstall:
	rm -f $(LUA_SHAREDIR)/apr.lua
	rm -fR $(LUA_SHAREDIR)/apr/test
	rm -fR $(LUA_LIBDIR)/apr

# Run the test suite.
test: $(BINARY_MODULE)
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

# Generate HTML documentation from Markdown embedded in source code.
docs: etc/docs.lua $(SOURCE_MODULE) $(SOURCES)
	@lua etc/docs.lua > docs.md
	@lua etc/wrap.lua < docs.md > docs.html

# Install the build dependencies using Debian/Ubuntu packages.
# FIXME The libreadline-dev isn't really needed here is it?!
install_deps:
	apt-get install libapr1 libapr1-dev libaprutil1 libaprutil1-dev \
		libaprutil1-dbd-sqlite3 libapreq2 libapreq2-dev lua5.1 \
		liblua5.1-0 liblua5.1-0-dev libreadline-dev luarocks
	luarocks install lua-discount
	luarocks install http://peterodding.com/code/lua/lxsh/downloads/lxsh-0.6.1-1.rockspec

# Prepare a source ZIP archive and a Debian package 
package: zip_package rockspec deb_package

# Create a profiling build, run the test suite, generate documentation
# including test coverage, then clean the intermediate files.
package_prerequisites:
	@echo Collecting coverage statistics using profiling build
	@make --no-print-directory clean
	@export PROFILING=1; lua etc/buildbot.lua --local
	@echo Generating documentation including coverage statistics
	@make --no-print-directory docs
	@make --no-print-directory clean

# Prepare a source ZIP archive from which Lua/APR can be build.
zip_package: package_prerequisites
	@rm -f $(PACKAGE).zip
	@mkdir -p $(PACKAGE)/doc
	@cp docs.html $(PACKAGE)/doc/apr.html
	@lua etc/wrap.lua < README.md > $(PACKAGE)/doc/readme.html
	@lua etc/wrap.lua < NOTES.md > $(PACKAGE)/doc/notes.html
	@lua etc/wrap.lua < TODO.md > $(PACKAGE)/doc/todo.html
	@mkdir -p $(PACKAGE)/etc
	@cp -a etc/buildbot.lua etc/docs.lua etc/errors.lua etc/wrap.lua $(PACKAGE)/etc
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

# Create a Debian package using "checkinstall".
deb_package: package_prerequisites
	@echo "Lua/APR is a binding to the Apache Portable Runtime (APR) library." > description-pak
	@echo "APR powers software such as the Apache webserver and Subversion and" >> description-pak
	@echo "Lua/APR makes the APR operating system interfaces available to Lua." >> description-pak
	@make --no-print-directory clean
	@make --no-print-directory DO_RELEASE=1
	checkinstall \
		--default \
		--backup=no \
		--type=debian \
		--pkgname=lua-apr \
		--pkgversion=$(VERSION) \
		--pkgrelease=$(RELEASE) \
		--pkglicense=MIT \
		--pkggroup=interpreters \
		'--requires=libapr1,libaprutil1,libaprutil1-dbd-sqlite3,libapreq2' \
		'--maintainer="Peter Odding <peter@peterodding.com>"' \
		make install LUA_DIR=/usr
	@rm -R description-pak doc-pak/

# Clean generated files from working directory.
clean:
	@rm -Rf $(BINARY_MODULE) $(OBJECTS) etc/coverage
	@rm -f $(APREQ_BINARY)
	@lcov -q -z -d . || true
	@rm -f src/*.gcov src/*.gcno

.PHONY: install uninstall test valgrind coverage docs install_deps package \
	package_prerequisites zip_package rockspec deb_package clean

# vim: ts=4 sw=4 noet
