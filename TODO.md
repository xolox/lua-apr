# To-do list for the Lua/APR binding

## New features

 * Make it possible to enable text mode for files, pipes and sockets on platforms where there is no distinction between text/binary mode (because `CR` + `LF` → `LF` translation can be useful on UNIX as well)
 * Support for **encrypted network communication**. It appears that APR itself doesn't support this but clearly it's possible because there are dozens of projects that use APR and support encrypted network communication (the [Apache HTTP server](http://en.wikipedia.org/wiki/Apache_HTTP_Server), [ApacheBench](http://en.wikipedia.org/wiki/ApacheBench), [Tomcat](http://en.wikipedia.org/wiki/Apache_Tomcat), etc.)

## Known problems

 * Find out why **`apr.xlate()` doesn't work on Windows** (I can't seem to get `apr_iconv` working on Windows)
 * Investigate **escaping problem in `apr_proc_create()`** as found by the test for the `apr.namedpipe_create()` function (see `etc/tests.lua` around line 625)
