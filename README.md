# Apache Portable Runtime <br> binding for Lua

The [Lua/APR binding](http://peterodding.com/code/lua/apr/) aims to bring most of the functionality in the [Apache Portable Runtime](http://en.wikipedia.org/wiki/Apache_Portable_Runtime) library to the small and flexible programming language [Lua](http://en.wikipedia.org/wiki/Lua_%28programming_language%29). Thus far the following modules have been implemented (the following links take you straight to the relevant documentation):

 * [Base64 encoding](http://peterodding.com/code/lua/apr/docs.html#base64_encoding)
 * [Cryptography routines](http://peterodding.com/code/lua/apr/docs.html#cryptography_routines)
 * [Environment manipulation](http://peterodding.com/code/lua/apr/docs.html#environment_manipulation)
 * [File path manipulation](http://peterodding.com/code/lua/apr/docs.html#file_path_manipulation)
 * [Filename matching](http://peterodding.com/code/lua/apr/docs.html#filename_matching)
 * [Directory manipulation](http://peterodding.com/code/lua/apr/docs.html#directory_manipulation)
 * [File I/O handling](http://peterodding.com/code/lua/apr/docs.html#file_i_o_handling) *(see status below)*
 * [String routines](http://peterodding.com/code/lua/apr/docs.html#string_routines)
 * [Time routines](http://peterodding.com/code/lua/apr/docs.html#time_routines)
 * [Uniform resource identifier parsing](http://peterodding.com/code/lua/apr/docs.html#uniform_resource_identifier_parsing)
 * [User/group identification](http://peterodding.com/code/lua/apr/docs.html#user_group_identification)
 * [Universally unique identifiers](http://peterodding.com/code/lua/apr/docs.html#universally_unique_identifiers)

## How to download, build & install the module

The project's [git repository](http://github.com/xolox/lua-apr) is hosted on [GitHub](http://github.com). To get you started on Linux (really any UNIX) try the following shell commands:

    $ git clone git://github.com/xolox/lua-apr.git
    $ cd lua-apr
    $ make install_deps # installs build & runtime dependencies for Debian/Ubuntu
    $ make install # installs apr.lua and apr/core.so in /usr/local
    $ make test # runs the test suite (using Shake if available)

A makefile for Windows is also included in the repository. It has been tested on Windows XP with NMAKE from the free [Microsoft Visual C++ Express](http://www.microsoft.com/express/Downloads/#2010-Visual-CPP) 2005/2008 tool chain and requires the [Microsoft Windows SDK](http://en.wikipedia.org/wiki/Microsoft_Windows_SDK#Obtaining_the_SDK). If you don't have git installed you can [download the latest sources](http://github.com/xolox/lua-apr/zipball/master) directly from GitHub as a ZIP file. Please note that the Windows makefile only builds the Lua/APR binding, you need to build APR yourself ([instructions](http://apr.apache.org/compiling_win32.html)).

## Status

The following functionality has not been implemented yet but is on the todo list (highest to lowest priority):

 * At the moment the file I/O module contains everything except **actual input/output handling**... This requires me to reimplement Lua's file I/O on top of APR. While I'm at it I also want to make this code flexible enough to support pipes and sockets because I certainly don't want to implement it more than once.

 * I have a prototype of a [**process handling module**](http://apr.apache.org/docs/apr/trunk/group__apr__thread__proc.html) but it's kind of useless until the file I/O interface is finished and works for pipes.

 * I want to add support for the [**I18N translation library**](http://apr.apache.org/docs/apr/trunk/group___a_p_r___x_l_a_t_e.html).

 * I've never used the APR [**network sockets**](http://apr.apache.org/docs/apr/trunk/group__apr__network__io.html) API but once I get to grips with it I'd like to support it in the binding. On the other hand it does seem one of the largest modules in APR.

## Contact

If you have questions, bug reports, suggestions, etc. the author can be contacted at <peter@peterodding.com>. The latest version is available at <http://peterodding.com/code/lua/apr/> and <http://github.com/xolox/lua-apr>.

## License

This software is licensed under the [MIT license](http://en.wikipedia.org/wiki/MIT_License).  
© 2010 Peter Odding &lt;<peter@peterodding.com>&gt;.
