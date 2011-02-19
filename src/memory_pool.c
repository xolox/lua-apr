/* Global memory pool handling for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: February 19, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 */

#include "lua_apr.h"

typedef struct {
  /* FIXME Reference counting isn't actually used for the global memory pool;
   * it's just that it's required by new_object() and related functions. */
  lua_apr_refobj header;
  apr_pool_t *pool;
} lua_apr_gpool;

lua_apr_objtype lua_apr_pool_type;

/* Initialize and/or retrieve the global memory pool from Lua's registry (every
 * Lua state has it's own global memory pool). */

apr_pool_t *to_pool(lua_State *L)
{
  lua_apr_gpool *reference;
  apr_pool_t *memory_pool;
  apr_status_t status;

  luaL_checkstack(L, 1, "not enough stack space to get memory pool");
  lua_pushlightuserdata(L, &lua_apr_pool_type); /* push anchor */
  lua_rawget(L, LUA_REGISTRYINDEX); /* pop anchor, push result */
  if (!lua_isuserdata(L, -1)) {
    lua_pop(L, 1); /* pop nil result */
    status = apr_pool_create(&memory_pool, NULL);
    if (status != APR_SUCCESS)
      raise_error_status(L, status);
    lua_pushlightuserdata(L, &lua_apr_pool_type); /* push anchor */
    reference = new_object(L, &lua_apr_pool_type);
    reference->pool = memory_pool;
    lua_rawset(L, LUA_REGISTRYINDEX); /* pop pool, anchor */
  } else {
    reference = lua_touserdata(L, -1);
    memory_pool = reference->pool;
    apr_pool_clear(memory_pool);
    lua_pop(L, 1); /* pop result */
  }

  return memory_pool;
}

/* Destroy the global memory pool, wait for any child threads to terminate. */

static int pool_gc(lua_State *L)
{
  lua_apr_gpool *gpool;

# if APR_HAS_THREADS
  threads_terminate();
# endif

  gpool = check_object(L, 1, &lua_apr_pool_type);
  apr_pool_destroy(gpool->pool);

  return 0;
}

static luaL_Reg pool_metamethods[] = {
  { "__gc", pool_gc },
  { NULL, NULL }
};

lua_apr_objtype lua_apr_pool_type = {
  "lua_apr_pool*",       /* metatable name in registry */
  "memory pool",         /* friendly object name */
  sizeof(lua_apr_gpool), /* structure size */
  NULL,                  /* methods table */
  pool_metamethods       /* metamethods table */
};
