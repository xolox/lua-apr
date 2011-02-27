# Notes about the Lua/APR binding

## APR documentation

The [online API documentation for APR](http://apr.apache.org/docs/apr/trunk/modules.html) has been my most useful resource while working on the Lua/APR binding but it leaves something to be desired when you're looking for a high level overview. The following online resources help in this regard:

 * [God's gift to C](http://www.theregister.co.uk/2006/04/27/gift_to_c/) is an introductory article to the functionality of APR by [The Register](http://www.theregister.co.uk/).
 * The [libapr tutorial by Inoue Seiichiro](http://dev.ariel-networks.com/apr/apr-tutorial/html/apr-tutorial.html) is a useful starting point for writing code.

## Building APR on Windows

Building APR on Windows can be a pain in the ass. It is meant to be done with Microsoft tools but fortunately these are freely available. Here are some notes I made in the process:

 1. Install [Microsoft Visual C++ Express](http://www.microsoft.com/express/Downloads/#2010-Visual-CPP). You only need the command line tools, the GUI isn't needed.

 2. Install the [Microsoft Platform SDK](http://en.wikipedia.org/wiki/Microsoft_Windows_SDK#Obtaining_the_SDK). The full SDK is over 1 GB but you only need the following:

    * Microsoft Windows Core SDK
      * Build environment (x86 32-bit)
    * Microsoft Web Workshop (IE) SDK
      * Build environment

 3. Download the APR, APR-util and APR-iconv archives (I used `apr-1.4.2-win32-src.zip`, `apr-util-1.3.9-win32-src.zip` and `apr-iconv-1.2.1-win32-src-r2.zip`) from [apr.apache.org](http://apr.apache.org/). Unpack all archives to the same directory and rename the subdirectories to `apr`, `apr-util` and `apr-iconv`.

 4. *The instructions about [building APR on Windows](http://apr.apache.org/compiling_win32.html) don't work for me so this is where things get sketchy:* Open a Windows SDK command prompt and navigate to the `apr-util` directory. Inside this directory execute `nmake -f Makefile.win buildall`. This doesn't work for me out of the box because of what's probably a bug in the APR-util makefile; I needed to replace `apr_app` with `aprapp` on lines 176 and 177 of `Makefile.win`. After this change `nmake` still exits with errors but nevertheless seems to build `libapr-1.dll` and `libaprutil-1.dll`...

### Building the SQlite3 database driver on Windows

The SQLite 3 driver is included in the [Windows binaries](http://github.com/downloads/xolox/lua-apr/lua-apr-0.11-win32.zip) but for the benefit of those who want to build the Apache Portable Runtime on Windows here are the steps involved:

 1. Download the [precompiled SQLite 3 binaries For Windows](http://www.sqlite.org/sqlite-dll-win32-x86-3070400.zip) (273.98 KiB) and unpack the files somewhere

 2. Create `sqlite3.lib` from `sqlite3.def` (included in the precompiled binaries) using the command `lib /machine:i386 /def:sqlite3.def` and copy `sqlite3.lib` to `apr-util-1.3.9/LibR`

 3. Download the corresponding [source code distribution](http://www.sqlite.org/sqlite-preprocessed-3070400.zip) (1.20 MiB) and copy `sqlite3.h` to `apr-util-1.3.9/include`

 4. Build the driver in the Windows SDK command prompt using the command `nmake /f apr_dbd_sqlite3.mak`

 5. To install the driver you can copy `sqlite3.dll` and `apr_dbd_sqlite3-1.dll` to Lua's installation directory

### Building `libapreq2` on Windows

I wasted a few hours getting `libapreq2` version 2.13 to build on Windows because of the following issues:

 * The included makefile `libapreq2.mak` is full of syntax errors
 * The makefile unconditionally includes the Apache module and consequently doesn't link without a full Apache build
 * The build environment requires a specific flavor of Perl which I haven't gotten to work

Eventually I decided to just rewrite the damned makefile and be done with it, enabling me to finally test the HTTP request parsing module on Windows (all tests passed the first time). I've included the [customized makefile](https://github.com/xolox/lua-apr/blob/master/etc/libapreq2.mak) in the Lua/APR git repository.
