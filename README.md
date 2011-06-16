# Apache Portable Runtime binding for Lua

[Lua/APR][homepage] is a binding of the [Apache Portable Runtime][wp_apr] (APR) for the [Lua programming language][wp_lua]. APR powers software such as the Apache webserver and Subversion and Lua/APR makes the APR operating system interfaces available to Lua, serving as an extended standard library. Thus far the following modules have been implemented (the following links take you straight to the relevant documentation):

[homepage]: http://peterodding.com/code/lua/apr/
[wp_apr]: http://en.wikipedia.org/wiki/Apache_Portable_Runtime
[wp_lua]: http://en.wikipedia.org/wiki/Lua_(programming_language)

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
 * [Memcached client](http://peterodding.com/code/lua/apr/docs/#memcached_client)
 * [Command argument parsing](http://peterodding.com/code/lua/apr/docs/#command_argument_parsing)
 * [HTTP request parsing](http://peterodding.com/code/lua/apr/docs/#http_request_parsing)
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

## How to get and install the module

You can find the source code of the most recently released version under [downloads][srcdl]. The source code is also available online in the [GitHub repository][github]. There are [Windows binaries][winbin] available that have been tested with [Lua for Windows][lfw] v5.1.4-40. You can also build the Lua/APR binding yourself. Here are your options:

[srcdl]: http://peterodding.com/code/lua/apr/downloads
[github]: http://github.com/xolox/lua-apr
[winbin]: http://peterodding.com/code/lua/apr/downloads/lua-apr-0.18-2-win32.zip
[lfw]: http://code.google.com/p/luaforwindows/

### Install the Debian/Ubuntu package

I've setup an experimental Debian package repository to make it easier to install the Lua/APR binding on Debian and Ubuntu. The following commands should help you get started ([see here for a detailed explanation][debrepo]):

    sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 6CD27CBF
    sudo sh -c 'echo deb http://peterodding.com/code/lua/apr/packages ./ >> /etc/apt/sources.list'
    sudo apt-get update
    sudo apt-get install --no-install-recommends lua-apr
    lua -e "require 'apr.test' ()"

[debrepo]: http://peterodding.com/code/lua/apr/packages/

### Build on UNIX using LuaRocks

The easiest way to download, build & install the Lua/APR binding is to use [LuaRocks] [lr]:

    $ luarocks install http://peterodding.com/code/lua/apr/downloads/lua-apr-0.18-2.rockspec

If you have git installed you can also download and install the latest sources using the following command:

    $ luarocks install http://peterodding.com/code/lua/apr/downloads/lua-apr-scm-1.rockspec

After installing the library you can run the test suite using the following command:

    $ lua -e "require 'apr.test' ()"

[lr]: http://luarocks.org/

### Build on UNIX using makefile

If you don't have LuaRocks installed the following shell commands should help you get started on UNIX:

    $ if which git; then # Get the latest sources using `git'?
    $   git clone git://github.com/xolox/lua-apr.git
    $ else # Or get the most recently released archive using `wget'.
    $   wget http://peterodding.com/code/lua/apr/downloads/lua-apr-0.18-2.zip
    $   unzip lua-apr-0.18-2.zip
    $   mv lua-apr-0.18-2 lua-apr
    $ fi
    $ cd lua-apr
    $ sudo make install_deps # installs build & runtime dependencies for Debian/Ubuntu
    $ sudo make install # installs apr.lua and apr/core.so in /usr/local
    $ lua -e "require 'apr.test' ()" # runs the test suite

The makefile creates a debug build by default. Use `sudo make install DO_RELEASE=1` to create a release build instead. Just be sure to run `make clean` when switching between debug/release mode to avoid linking incompatible object files.

### Build on Windows using makefile

A makefile for Microsoft Windows is included in the repository. It has been tested on Windows XP with `NMAKE` from the free [Microsoft Visual C++ Express][msvc] 2010 tool chain. If you don't have git installed you can [download the latest sources][autozip] directly from GitHub as a ZIP file. Please note that the Windows makefile only builds the Lua/APR binding, you need to build APR yourself (see the [instructions][apr_build]).

The makefile creates a debug build by default. Use `NMAKE /f Makefile.win DO_RELEASE=1` to create a release build instead. Just be sure to run `NMAKE /f Makefile.win clean` when switching between debug/release mode to avoid linking incompatible object files.

[msvc]: http://www.microsoft.com/express/Downloads/#2010-Visual-CPP
[autozip]: http://github.com/xolox/lua-apr/zipball/master
[apr_build]: http://apr.apache.org/compiling_win32.html

## Status

The following functionality has not been implemented yet but is on the to-do list:

 * **Encrypted network communication** (unfortunately this isn't provided by APR so [io_net.c][io_net] could get messy…)
 * zhiguo zhao contributed an **LDAP** module which will soon be integrated in the Lua/APR binding

[io_net]: https://github.com/xolox/lua-apr/blob/master/src/io_net.c

## Contact

If you have questions, bug reports, suggestions, etc. the author can be contacted at <peter@peterodding.com>. The latest version is available at <http://peterodding.com/code/lua/apr/> and <http://github.com/xolox/lua-apr>.

## License

This software is licensed under the [MIT license](http://en.wikipedia.org/wiki/MIT_License).  
© 2011 Peter Odding (<peter@peterodding.com>) and zhiguo zhao (<zhaozg@gmail.com>).
