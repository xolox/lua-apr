/* Environment manipulation module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: September 19, 2010
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * Operating systems organize tasks into processes. One of the simplest means
 * of communication between processes is the use of environment variables. If
 * you're not familiar with environment variables, picture every process on
 * your computer having an associated Lua table with string key/value pairs.
 * When a process creates or overwrites a key/value pair only the table of that
 * process changes. When the process spawns a child process the child gets a
 * copy of the table which from that point onwards is no longer associated with
 * the parent process.
 */

#include "lua_apr.h"
#include <apr_env.h>

/* apr.env_get(name) -> value {{{1
 *
 * Get the value of the environment variable @name. On success the string value
 * is returned. If the variable does not exist nothing is returned. Otherwise a
 * nil followed by an error message is returned.
 */

int lua_apr_env_get(lua_State *L)
{
  apr_pool_t *memory_pool;
  apr_status_t status;
  const char *name;
  char *value;

  memory_pool = to_pool(L);
  name = luaL_checkstring(L, 1);
  status = apr_env_get(&value, name, memory_pool);
  if (APR_STATUS_IS_ENOENT(status)) {
    return 0;
  } else if (status != APR_SUCCESS) {
    return push_error_status(L, status);
  } else {
    lua_pushstring(L, value);
    return 1;
  }
}

/* apr.env_set(name, value) -> status {{{1
 *
 * Set the value of the environment variable @name to the string @value. On
 * success true is returned, otherwise a nil followed by an error message is
 * returned.
 */

int lua_apr_env_set(lua_State *L)
{
  const char *name, *value;
  apr_pool_t *memory_pool;
  apr_status_t status;

  memory_pool = to_pool(L);
  name = luaL_checkstring(L, 1);
  value = luaL_checkstring(L, 2);
  status  = apr_env_set(name, value, memory_pool);

  return push_status(L, status);
}

/* apr.env_delete(name) -> status {{{1
 *
 * Delete the environment variable @name. On success true is returned,
 * otherwise a nil followed by an error message is returned.
 */

int lua_apr_env_delete(lua_State *L)
{
  apr_pool_t *memory_pool;
  apr_status_t status;
  const char *name;

  memory_pool = to_pool(L);
  name = luaL_checkstring(L, 1);
  status = apr_env_delete(name, memory_pool);

  return push_status(L, status);
}

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
