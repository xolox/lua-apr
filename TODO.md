# To-do list for the Lua/APR binding

## New features

 * Make it possible to enable text mode for files, pipes and sockets on platforms where there is no distinction between text/binary mode (because `CR` + `LF` → `LF` translation can be useful on UNIX as well)
 * Support for **encrypted network communication**. It appears that APR itself doesn't support this but clearly it's possible because there are dozens of projects that use APR and support encrypted network communication (the [Apache HTTP server](http://en.wikipedia.org/wiki/Apache_HTTP_Server), [ApacheBench](http://en.wikipedia.org/wiki/ApacheBench), [Tomcat](http://en.wikipedia.org/wiki/Apache_Tomcat), etc.)

## Known problems

 * Find out why **`apr.xlate()` doesn't work on Windows** (I can't seem to get `apr_iconv` working on Windows)
 * Investigate **escaping problem in `apr_proc_create()`** as found by the test for the `apr.namedpipe_create()` function (see `etc/tests.lua` around line 625)
 * Why is the DBD `LD_PRELOAD` trick needed?! [More information](https://answers.launchpad.net/ubuntu/+source/apr-util/+question/143914)
 * When the Lua interpreter exits because of an error just after calling `apr.thread_create()`, the process can crash with a segmentation fault. This depends a lot on timing and only happens once in about 50 tries on my machine. So far I've managed to reproduce the crash using a [GDB](http://en.wikipedia.org/wiki/GNU_Debugger) session scripted with [Expect](http://en.wikipedia.org/wiki/Expect).. The currently released code already includes [a mutex to wait for all child threads to exit](https://github.com/xolox/lua-apr/blob/master/src/thread.c#L435) but this doesn't always work -- it seems that sometimes the mutex is destroyed (outside of the Lua/APR binding's control) before the child thread is completely terminated (either that or a lot of stuff happens after calling `apr_thread_exit()`..)

## Anything else?

 * Propose the [libapreq2 binding](https://github.com/xolox/lua-apr/blob/master/src/http.c) for inclusion as the official Lua language binding of [libapreq2](http://httpd.apache.org/apreq/)? (first make the binding a lot more complete)
 * Integrate the LDAP binding contributed by zhiguo zhao!
