/* Global memory pool handling for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: June 16, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 */

#include "lua_apr.h"

/* Destroy the global memory pool, wait for any child threads to terminate. */

static int pool_gc(lua_State *L)
{
  apr_pool_t *pool;

  pool = *(apr_pool_t**)luaL_checkudata(L, 1, LUA_APR_POOL_MT);
  apr_pool_destroy(pool);

  return 0;
}

/* Initialize and/or retrieve the global memory pool from Lua's registry
 * (every Lua state has it's own global memory pool). */

apr_pool_t *to_pool(lua_State *L)
{
  apr_pool_t *memory_pool;
  apr_status_t status;

  luaL_checkstack(L, 1, "not enough stack space to get memory pool");
  lua_getfield(L, LUA_REGISTRYINDEX, LUA_APR_POOL_KEY);
  if (!lua_isuserdata(L, -1)) {
    lua_pop(L, 1);
    status = apr_pool_create(&memory_pool, NULL);
    if (status != APR_SUCCESS)
      raise_error_status(L, status);
    *(apr_pool_t**)lua_newuserdata(L, sizeof(apr_pool_t*)) = memory_pool;
    if (luaL_newmetatable(L, LUA_APR_POOL_MT)) {
      lua_pushcfunction(L, pool_gc);
      lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);
    lua_setfield(L, LUA_REGISTRYINDEX, LUA_APR_POOL_KEY);
  } else {
    memory_pool = *(apr_pool_t**)lua_touserdata(L, -1);
    apr_pool_clear(memory_pool);
    lua_pop(L, 1);
  }

  return memory_pool;
}
