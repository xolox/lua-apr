# Apache Portable Runtime <br> binding for Lua

The [Lua/APR binding](http://peterodding.com/code/lua/apr/) aims to bring most of the functionality in the [Apache Portable Runtime](http://en.wikipedia.org/wiki/Apache_Portable_Runtime) library to the small and flexible programming language [Lua](http://en.wikipedia.org/wiki/Lua_%28programming_language%29). Thus far the following modules have been implemented (the following links take you straight to the relevant documentation):

 * [Base64 encoding](http://peterodding.com/code/lua/apr/docs/#base64_encoding)
 * [Cryptography routines](http://peterodding.com/code/lua/apr/docs/#cryptography_routines)
 * [Date parsing](http://peterodding.com/code/lua/apr/docs/#date_parsing)
 * [Relational database drivers](http://peterodding.com/code/lua/apr/docs/#relational_database_drivers)
 * [DBM routines](http://peterodding.com/code/lua/apr/docs/#dbm_routines)
 * [Environment manipulation](http://peterodding.com/code/lua/apr/docs/#environment_manipulation)
 * [File path manipulation](http://peterodding.com/code/lua/apr/docs/#file_path_manipulation)
 * [Filename matching](http://peterodding.com/code/lua/apr/docs/#filename_matching)
 * [Directory manipulation](http://peterodding.com/code/lua/apr/docs/#directory_manipulation)
 * [File I/O handling](http://peterodding.com/code/lua/apr/docs/#file_i_o_handling)
 * [Network I/O handling](http://peterodding.com/code/lua/apr/docs/#network_i_o_handling)
 * [Pipe I/O handling](http://peterodding.com/code/lua/apr/docs/#pipe_i_o_handling)
 * [Command argument parsing](http://peterodding.com/code/lua/apr/docs/#command_argument_parsing)
 * [Process handling](http://peterodding.com/code/lua/apr/docs/#process_handling)
 * [Shared memory](http://peterodding.com/code/lua/apr/docs/#shared_memory)
 * [String routines](http://peterodding.com/code/lua/apr/docs/#string_routines)
 * [Multi threading](http://peterodding.com/code/lua/apr/docs/#multi_threading)
 * [Thread queues](http://peterodding.com/code/lua/apr/docs/#thread_queues)
 * [Time routines](http://peterodding.com/code/lua/apr/docs/#time_routines)
 * [Uniform resource identifier parsing](http://peterodding.com/code/lua/apr/docs/#uniform_resource_identifier_parsing)
 * [User/group identification](http://peterodding.com/code/lua/apr/docs/#user_group_identification)
 * [Universally unique identifiers](http://peterodding.com/code/lua/apr/docs/#universally_unique_identifiers)
 * [Character encoding translation](http://peterodding.com/code/lua/apr/docs/#character_encoding_translation)
 * [XML parsing](http://peterodding.com/code/lua/apr/docs/#xml_parsing)

## How to download, build & install the module

There are [Windows binaries](http://peterodding.com/code/lua/apr/downloads/lua-apr-0.14.2-win32.zip) available that have been tested with [Lua for Windows](http://code.google.com/p/luaforwindows/) v5.1.4-40. You can also build the Lua/APR binding yourself. Here are your options:

### Build on UNIX using LuaRocks

The easiest way to download, build & install the Lua/APR binding is to use [LuaRocks](http://luarocks.org/):

    $ luarocks install http://peterodding.com/code/lua/apr/downloads/lua-apr-0.14.2-1.rockspec

If you have git installed you can also download and install the latest sources using the following command:

    $ luarocks install http://peterodding.com/code/lua/apr/downloads/lua-apr-scm-1.rockspec

After installing the library you can run the test suite using the following command:

    $ lua -lapr.test

### Build on UNIX using makefile

If you don't have LuaRocks installed the following shell commands should help you get started on UNIX:

    $ if which git; then # Get the latest sources using `git'?
    $   git clone git://github.com/xolox/lua-apr.git
    $ else # Or get the most recently released archive using `wget'.
    $   wget http://peterodding.com/code/lua/apr/downloads/lua-apr-0.14.2-1.zip
    $   unzip lua-apr-0.14.2-1.zip
    $   mv lua-apr-0.14.2-1 lua-apr
    $ fi
    $ cd lua-apr
    $ sudo make install_deps # installs build & runtime dependencies for Debian/Ubuntu
    $ sudo make install # installs apr.lua and apr/core.so in /usr/local
    $ lua -lapr.test # runs the test suite

The makefile creates a debug build by default. Use `sudo make install RELEASE=1` to create a release build instead. Just be sure to run `make clean` when switching between debug/release mode to avoid linking incompatible object files.

### Build on Windows using makefile

A makefile for Microsoft Windows is included in the repository. It has been tested on Windows XP with `NMAKE` from the free [Microsoft Visual C++ Express](http://www.microsoft.com/express/Downloads/#2010-Visual-CPP) 2010 tool chain. If you don't have git installed you can [download the latest sources](http://github.com/xolox/lua-apr/zipball/master) directly from GitHub as a ZIP file. Please note that the Windows makefile only builds the Lua/APR binding, you need to build APR yourself (see the [instructions](http://apr.apache.org/compiling_win32.html)).

The makefile creates a debug build by default. Use `NMAKE /f Makefile.win RELEASE=1` to create a release build instead. Just be sure to run `NMAKE /f Makefile.win clean` when switching between debug/release mode to avoid linking incompatible object files.

## Status

The following functionality has not been implemented yet but is on the to-do list:

 * **Encrypted network communication** (unfortunately this isn't provided by APR so [io_net.c](https://github.com/xolox/lua-apr/blob/master/src/io_net.c) could get messy…)

## Contact

If you have questions, bug reports, suggestions, etc. the author can be contacted at <peter@peterodding.com>. The latest version is available at <http://peterodding.com/code/lua/apr/> and <http://github.com/xolox/lua-apr>.

## License

This software is licensed under the [MIT license](http://en.wikipedia.org/wiki/MIT_License).  
© 2011 Peter Odding &lt;<peter@peterodding.com>&gt; and zhiguo zhao &lt;<zhaozg@gmail.com>&gt;.
