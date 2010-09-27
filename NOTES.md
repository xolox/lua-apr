# Notes about the Lua/APR binding

## APR documentation

The [online API documentation for APR](http://apr.apache.org/docs/apr/trunk/modules.html) has been my most useful resource while working on the Lua/APR binding but it leaves something to be desired when you're looking for a high level overview. The following online resources help in this regard:

 * [God's gift to C](http://www.theregister.co.uk/2006/04/27/gift_to_c/) is an introductory article to the functionality of APR by [The Register](http://www.theregister.co.uk/).
 * The [libapr tutorial by Inoue Seiichiro](http://dev.ariel-networks.com/apr/apr-tutorial/html/apr-tutorial.html) is a useful starting point for writing code.

## Building APR on Windows

Building APR on Windows can be a pain in the ass. It is meant to be done with Microsoft tools but fortunately these are freely available. Here are some notes I made in the process:

 1. Install [Microsoft Visual C++ Express](http://www.microsoft.com/express/Downloads/#2010-Visual-CPP). You only need the command line tools, the GUI isn't needed.

 2. Install the [Microsoft Platform SDK](http://en.wikipedia.org/wiki/Microsoft_Windows_SDK#Obtaining_the_SDK). The full SDK is over 1 GB but you only need the following:

  + Microsoft Windows Core SDK
   - Build environment (x86 32-bit)
  + Microsoft Web Workshop (IE) SDK
   - Build environment

 3. Download APR, APR-util and APR-iconv archives from <http://apr.apache.org/>. Unpack all archives to the same directory and rename the subdirectories to `apr`, `apr-util` and `apr-iconv`.

 4. *The instructions about [building APR on Windows](http://apr.apache.org/compiling_win32.html) don't work for me so this is where things get sketchy:* Open a Visual Studio command prompt and navigate to the `apr-util` directory. Inside this directory execute `nmake -f Makefile.win buildall`. This doesn't work for me out of the box because of what's probably a bug in the APR-util makefile; I needed to replace `apr_app` with `aprapp` on lines 176 and 177 of `Makefile.win`. After this change `nmake` still exits with errors but nevertheless seems to build `libapr-1.dll` and `libaprutil-1.dll`...
