# To-do list for the Lua/APR binding

 * Make object methods local to files (see for example [io_net.c](https://github.com/xolox/lua-apr/blob/master/src/io_net.c)).
 * Include an `apr.type()` function that returns strings like `'file'`, `'socket'`, `'process'`?
 * Always include userdata addresses in `__tostring()` output to ease debugging of e.g. closed files.
 * Find a way to implement *useful* multi threading on top of APR:
   * My primary use case is a multi threaded network server which immediately introduces complex problems like how to move a client socket into a new thread once it has been created in the main server thread using [socket:accept()](http://peterodding.com/code/lua/apr/docs/#socket:accept)… :-(
   * The [Lua Lanes library](http://kotisivu.dnainternet.net/askok/bin/lanes/) was inspired by the [Linda coordination model](http://en.wikipedia.org/wiki/Linda_%28coordination_language%29). APR probably implements all the primitives I would need to build something similar, find out if this is feasible?
