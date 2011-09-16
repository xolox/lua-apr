# To-do list for the Lua/APR binding

## New features

 * **LDAP directory modification** (creating, changing and deleting LDAP directory entries)
 * **Asynchronous network communication** (a binding for the [pollset][pollset] module?)
 * **Encrypted network communication**. It appears that APR itself doesn't support this but clearly it's possible because there are dozens of projects that use APR and support encrypted network communication (the [Apache HTTP server] [httpd], [ApacheBench] [ab], [Tomcat] [tomcat], etc.)
 * Make it possible to enable text mode for files, pipes and sockets on platforms where there is no distinction between text/binary mode (because `CR` + `LF` → `LF` translation can be useful on UNIX as well)
 
[pollset]: http://apr.apache.org/docs/apr/trunk/group__apr__poll.html
[httpd]: http://en.wikipedia.org/wiki/Apache_HTTP_Server
[ab]: http://en.wikipedia.org/wiki/ApacheBench
[tomcat]: http://en.wikipedia.org/wiki/Apache_Tomcat

## Known problems

 * Find out why **`apr.xlate()` doesn't work on Windows** (I can't seem to get `apr_iconv` working on Windows)
 * Investigate **escaping problem in `apr_proc_create()`** as found by the test for the `apr.namedpipe_create()` function (see `etc/tests.lua` around line 625)
 * Why is the DBD `LD_PRELOAD` trick needed?! [More information] [dbd_trick]

[dbd_trick]: https://answers.launchpad.net/ubuntu/+source/apr-util/+question/143914

## Anything else?

 * Propose the [libapreq2 binding] [apreq_binding] for inclusion as the official Lua language binding of [libapreq2] [libapreq2]? (first make the binding a lot more complete)
 * [Maybe][atexit] I shouldn't be using `atexit()` to call `apr_terminate()`? (BTW the whole linked blog post is interesting, as is the follow-up post)

[apreq_binding]: https://github.com/xolox/lua-apr/blob/master/src/http.c
[libapreq2]: http://httpd.apache.org/apreq/
[atexit]: http://davidz25.blogspot.com/2011/06/writing-c-library-part-1.html
