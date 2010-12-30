# To-do list for the Lua/APR binding

 * Support for **encrypted network communication**. It appears that APR itself doesn't support this but clearly it's possible because there are dozens of projects that use APR and support encrypted network communication (the [Apache HTTP server](http://en.wikipedia.org/wiki/Apache_HTTP_Server), [ApacheBench](http://en.wikipedia.org/wiki/ApacheBench), [Tomcat](http://en.wikipedia.org/wiki/Apache_Tomcat), etc.)
 * Find a way to implement **useful multi threading** on top of APR:
   * My primary use case is a multi threaded network server which immediately introduces complex problems like how to move a client socket into a new thread once it has been created in the main server thread using [socket:accept()](http://peterodding.com/code/lua/apr/docs/#socket:accept)… :-(
   * The [Lua Lanes library](http://kotisivu.dnainternet.net/askok/bin/lanes/) was inspired by the [Linda coordination model](http://en.wikipedia.org/wiki/Linda_%28coordination_language%29). APR probably implements all the primitives I would need to build something similar, find out if this is feasible?
 * Investigate **escaping problem in `apr_proc_create()`** as found by the test for the `apr.namedpipe_create()` function (see `etc/tests.lua` around line 625)
