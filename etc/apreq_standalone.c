/* Standalone libapreq2 binding based on the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: February 27, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * This file transforms the HTTP request parsing module for the Lua/APR binding
 * into a standalone binding to libapreq2, making it a lighter dependency for
 * other web projects.
 */

#include "../src/lua_apr.h"

/* Make memory_pool.c use separate memory pool for standalone APREQ module. */
#undef LUA_APR_POOL_MT
#define LUA_APR_POOL_MT "APREQ memory pool type"
#undef LUA_APR_POOL_KEY
#define LUA_APR_POOL_KEY "APREQ memory pool instance"
#define APREQ_STANDALONE

/* Dependencies copied from Lua/APR. */

int status_to_message(lua_State *L, apr_status_t status)
{
  char message[LUA_APR_MSGSIZE];
  apr_strerror(status, message, count(message));
  lua_pushstring(L, message);
  return 1;
}

int push_error_status(lua_State *L, apr_status_t status)
{
  lua_pushnil(L);
  status_to_message(L, status);
  status_to_name(L, status);
  return 3;
}

#include "../src/errno.c"
#include "../src/memory_pool.c"
#include "../src/http.c"

/* Module loader. */

/* Used to make sure that APR is only initialized once. */
static int apr_was_initialized = 0;

LUA_APR_EXPORT int luaopen_apreq(lua_State *L)
{
  apr_status_t status;

  /* The Lua/APR binding probably isn't loaded which makes the APREQ binding
   * responsible for correctly initializing the Apache Portable Runtime. */
  if (!apr_was_initialized) {
    if ((status = apr_initialize()) != APR_SUCCESS)
      raise_error_status(L, status);
    if (atexit(apr_terminate) != 0)
      raise_error_message(L, "APREQ: Failed to register apr_terminate()");
    apr_was_initialized = 1;
  }

  /* Create the memory pool for APREQ functions and install a __gc metamethod
   * to destroy the memory pool when the Lua state is closed. */
  to_pool(L);

  /* Create and return the module table. */
  lua_newtable(L);
# define push_libfun(f) \
    lua_pushcfunction(L, lua_apr_ ## f); \
    lua_setfield(L, -2, #f)
  push_libfun(parse_headers);
  push_libfun(parse_multipart);
  push_libfun(parse_cookie_header);
  push_libfun(parse_query_string);
  return 1;
}
