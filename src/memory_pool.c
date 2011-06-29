/* Global memory pool handling for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: June 30, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * Most functions in APR and APR-util require a memory pool argument to perform
 * memory allocation. Object methods in Lua/APR generally use the memory pool
 * associated with the object (such memory pools are destroyed when the object
 * userdata is garbage collected). For standalone functions Lua/APR uses a
 * global memory pool stored in the registry of the Lua state. This source file
 * implements the global memory pool.
 */

#include "lua_apr.h"

/* Destroy the global memory pool. */

static int pool_gc(lua_State *L)
{
  apr_pool_t **reference;

  reference = luaL_checkudata(L, 1, LUA_APR_POOL_MT);
  apr_pool_destroy(*reference);

  return 0;
}

/* Get the global memory pool (creating or clearing it). */

apr_pool_t *to_pool(lua_State *L)
{
  apr_pool_t *memory_pool, **reference;
  apr_status_t status;

  luaL_checkstack(L, 1, "not enough stack space to get memory pool");
  lua_getfield(L, LUA_REGISTRYINDEX, LUA_APR_POOL_KEY);
  if (!lua_isuserdata(L, -1)) {
    /* The global memory pool has not yet been created. */
    lua_pop(L, 1);
    status = apr_pool_create(&memory_pool, NULL);
    if (status != APR_SUCCESS)
      raise_error_status(L, status);
    reference = lua_newuserdata(L, sizeof memory_pool);
    *reference = memory_pool;
    if (luaL_newmetatable(L, LUA_APR_POOL_MT)) {
      /* The metatable has not yet been initialized. */
      lua_pushcfunction(L, pool_gc);
      lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);
    lua_setfield(L, LUA_REGISTRYINDEX, LUA_APR_POOL_KEY);
  } else {
    /* Clear and return the previously created global memory pool. */
    reference = lua_touserdata(L, -1);
    memory_pool = *reference;
    apr_pool_clear(memory_pool);
    lua_pop(L, 1);
  }

  return memory_pool;
}
