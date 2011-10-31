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
 * [LDAP connection handling](http://peterodding.com/code/lua/apr/docs/#ldap_connection_handling)
 * [Process handling](http://peterodding.com/code/lua/apr/docs/#process_handling)
 * [Shared memory](http://peterodding.com/code/lua/apr/docs/#shared_memory)
 * [Signal handling](http://peterodding.com/code/lua/apr/docs/#signal_handling)
 * [String routines](http://peterodding.com/code/lua/apr/docs/#string_routines)
 * [Multi threading](http://peterodding.com/code/lua/apr/docs/#multi_threading)
 * [Thread queues](http://peterodding.com/code/lua/apr/docs/#thread_queues)
 * [Time routines](http://peterodding.com/code/lua/apr/docs/#time_routines)
 * [Uniform resource identifier parsing](http://peterodding.com/code/lua/apr/docs/#uniform_resource_identifier_parsing)
 * [User/group identification](http://peterodding.com/code/lua/apr/docs/#user_group_identification)
 * [Universally unique identifiers](http://peterodding.com/code/lua/apr/docs/#universally_unique_identifiers)
 * [Character encoding translation](http://peterodding.com/code/lua/apr/docs/#character_encoding_translation)
 * [XML parsing](http://peterodding.com/code/lua/apr/docs/#xml_parsing)

## How to get and install the binding

You can find the source code of the most recently released version under [downloads][srcdl]. The source code is also available online in the [GitHub repository][github]. There are [Windows binaries][winbin] available that have been tested with [Lua for Windows][lfw] v5.1.4-40 and there is an offical Debian Linux package. You can also build the Lua/APR binding yourself using the instructions below.

[srcdl]: http://peterodding.com/code/lua/apr/downloads
[github]: http://github.com/xolox/lua-apr
[winbin]: http://peterodding.com/code/lua/apr/downloads/lua-apr-0.20-win32.zip
[lfw]: http://code.google.com/p/luaforwindows/

### Install using Debian package

The Lua/APR binding has an official Debian package which is available in [wheezy (testing)] [wheezy], [sid (unstable)] [sid] and Ubuntu [Oneiric Ocelot (11.10)] [oneiric]. If you are running any of those (or a newer release) the following commands should get you started:

    $ apt-get install liblua5.1-apr1
    $ lua -e "require 'apr.test' ()"

If you are running an older Debian release or a derivative of Debian which doesn't have the official package yet, you may be able to download and install one of the Debian packages manually from one of the three pages linked above.

[wheezy]: http://packages.debian.org/source/wheezy/lua-apr
[sid]: http://packages.debian.org/source/sid/lua-apr
[oneiric]: http://packages.ubuntu.com/source/oneiric/lua-apr

### Build on UNIX using LuaRocks

The easiest way to download, build and install the Lua/APR binding is to use [LuaRocks] [lr]. The following commands will install the Lua/APR binding from the main repository and run the test suite to make sure everything works:

    $ luarocks install lua-apr
    $ lua -e "require 'apr.test' ()"

If LuaRocks fails to build the Lua/APR binding this is likely because of missing dependencies. Lua/APR depends on the APR, APR-util and libapreq2 system libraries but LuaRocks cannot install these because it only deals with Lua modules. When the build fails the makefile runs a Lua script that knows how to detect missing dependencies on Debian, Ubuntu, Arch Linux, Red Hat, Fedora Core, Suse Linux and CentOS. As a general guideline for other systems and package managers, you'll need the binary and development packages of Lua 5.1, APR, APR-util and libapreq2.

The latest rockspec and sources are also available from the author's website (in case the main LuaRocks repository is unavailable or lagging behind):

    $ luarocks install http://peterodding.com/code/lua/apr/downloads/lua-apr-0.20.6-1.rockspec

If you have git installed you can also download and install the latest sources directly from [GitHub] [github]:

    $ luarocks install http://peterodding.com/code/lua/apr/downloads/lua-apr-scm-1.rockspec

[lr]: http://luarocks.org/

### Build on UNIX using makefile

If you don't have LuaRocks installed the following shell commands should help you get started on UNIX:

    $ if which git; then # Get the latest sources using `git'?
    $   git clone git://github.com/xolox/lua-apr.git
    $ else # Or get the most recently released archive using `wget'.
    $   wget http://peterodding.com/code/lua/apr/downloads/lua-apr-0.20.6-1.zip
    $   unzip lua-apr-0.20.6-1.zip
    $   mv lua-apr-0.20.6-1 lua-apr
    $ fi
    $ cd lua-apr
    $ sudo make install # installs apr.lua and apr/core.so in /usr/local
    $ lua -e "require 'apr.test' ()" # runs the test suite

The makefile creates a debug build by default. Use `sudo make install DO_RELEASE=1` to create a release build instead. Just be sure to run `make clean` when switching between debug/release mode to avoid linking incompatible object files.

### Build on Windows using makefile

A makefile for Microsoft Windows is included in the repository. It has been tested on Windows XP with `NMAKE` from the free [Microsoft Visual C++ Express][msvc] 2010 tool chain. If you don't have git installed you can [download the latest sources][autozip] directly from GitHub as a ZIP file.

At the top of the makefile several file locations are defined, you'll need to change these to match your system. The makefile creates a debug build by default. Use `NMAKE /f Makefile.win DO_RELEASE=1` to create a release build instead. Just be sure to run `NMAKE /f Makefile.win clean` when switching between debug/release mode to avoid linking incompatible object files.

Please note that the Windows makefile only builds the Lua/APR binding, you need to build the APR, APR-util and APREQ libraries yourself. There are instructions available on [how to build APR/APR-util on apache.org][apr_build] but [my notes on the process][notes] may be a more useful starting point.

I've also recently created a Windows batch script that bootstraps a Lua/APR development environment by downloading, unpacking, patching and building the libraries involved. To use it, download the [ZIP archive][bootstrap_zip], unpack it to a directory, open a Windows SDK command prompt in the directory where you unpacked the ZIP archive and execute `make.cmd`. I've only tested it on a 32 bit Windows XP virtual machine, but even if it doesn't work out of the box for you it may provide a useful starting point.

[msvc]: http://www.microsoft.com/express/Downloads/#2010-Visual-CPP
[autozip]: http://github.com/xolox/lua-apr/zipball/master
[apr_build]: http://apr.apache.org/compiling_win32.html
[notes]: https://github.com/xolox/lua-apr/blob/master/NOTES.md#readme
[bootstrap_zip]: http://peterodding.com/code/lua/apr/downloads/win32-bootstrap.zip

## Status

The following functionality has not been implemented yet but is on the to-do list:

 * **LDAP directory modification** (creating, changing and deleting LDAP directory entries)
 * **Asynchronous network communication** (a binding for the [pollset][pollset] module?)
 * **Encrypted network communication** (unfortunately this isn't provided by APR so [io_net.c][io_net] could get messy…)

[pollset]: http://apr.apache.org/docs/apr/trunk/group__apr__poll.html
[io_net]: https://github.com/xolox/lua-apr/blob/master/src/io_net.c

## Contact

If you have questions, bug reports, suggestions, etc. the author can be contacted at <peter@peterodding.com>. The latest version is available at <http://peterodding.com/code/lua/apr/> and <http://github.com/xolox/lua-apr>.

## License

This software is licensed under the [MIT license](http://en.wikipedia.org/wiki/MIT_License).  
© 2011 Peter Odding (<peter@peterodding.com>) and zhiguo zhao (<zhaozg@gmail.com>).

The LDAP connection handling module includes parts of LuaLDAP, designed and implemented by Roberto Ierusalimschy, André Carregal and Tomás Guisasola.  
© 2003-2007 The Kepler Project.
