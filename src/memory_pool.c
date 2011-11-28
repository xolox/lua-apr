/* Global memory pool handling for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: November 28, 2011
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

typedef struct {
  apr_pool_t *pool;
  int managed; /* should be cleared/destroyed by Lua/APR? */
} global_pool;

static apr_pool_t *pool_register(lua_State*, apr_pool_t*, int);
static int pool_gc(lua_State*);

/* to_pool() - Get the global memory pool (creating or clearing it). {{{1 */

apr_pool_t *to_pool(lua_State *L)
{
  global_pool *reference;
  apr_pool_t *memory_pool;
  apr_status_t status;

  luaL_checkstack(L, 1, "not enough stack space to get memory pool");
  lua_getfield(L, LUA_REGISTRYINDEX, LUA_APR_POOL_KEY);
  if (!lua_isuserdata(L, -1)) {
    /* Pop the nil value from lua_getfield(). */
    lua_pop(L, 1);
    /* Create the memory pool itself. */
    status = apr_pool_create(&memory_pool, NULL);
    if (status != APR_SUCCESS)
      raise_error_status(L, status);
    /* Create a reference to the memory pool in the Lua state. */
    (void) pool_register(L, memory_pool, 1);
  } else {
    /* Return the previously created global memory pool. */
    reference = lua_touserdata(L, -1);
    memory_pool = reference->pool;
    /* Clear it when we're allowed to do so. */
    if (reference->managed)
      apr_pool_clear(memory_pool);
    lua_pop(L, 1);
  }

  return memory_pool;
}

/* pool_register() - Initialize or replace the global memory pool. {{{1 */

apr_pool_t *pool_register(lua_State *L, apr_pool_t *new_pool, int managed)
{
  apr_pool_t *old_pool = NULL;
  global_pool *reference;

  /* Get the old global memory pool (if any). */
  lua_getfield(L, LUA_REGISTRYINDEX, LUA_APR_POOL_KEY);
  if (lua_isuserdata(L, -1)) {
    reference = lua_touserdata(L, -1);
    old_pool = reference->pool;
    /* We don't need this userdata on the stack. */
    lua_pop(L, 1);
  } else {
    /* Pop the nil value from lua_getfield(). */
    lua_pop(L, 1);
    /* Create the reference to the global memory pool. */
    reference = lua_newuserdata(L, sizeof *reference);
    reference->managed = managed;
    /* Create and install a metatable for garbage collection. */
    if (luaL_newmetatable(L, LUA_APR_POOL_MT)) {
      /* The metatable has not yet been initialized. */
      lua_pushcfunction(L, pool_gc);
      lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);
    /* Add the reference tot the Lua registry (popping it from the stack). */
    lua_setfield(L, LUA_REGISTRYINDEX, LUA_APR_POOL_KEY);
  }

  /* Should we change the memory pool? */
  if (new_pool != old_pool) {
    /* Just in case lua_apr_pool_register() is ever called after the global
     * memory pool has already been created. */
    if (reference->managed && old_pool != NULL) {
      /* Destroy the old _managed_ memory pool. */
      apr_pool_destroy(old_pool);
      /* Don't return it to the caller though. */
      old_pool = NULL;
    }
    /* Set the reference to the new pool. */
    reference->pool = new_pool;
  }

  /* Always set the managed field. */
  reference->managed = managed;

  /* Return the old memory pool (if any) in case
   * the caller created it and wants it back :-) */
  return old_pool;
}

/* pool_gc() - Destroy the global memory pool automatically. {{{1 */

int pool_gc(lua_State *L)
{
  global_pool *reference;

  reference = luaL_checkudata(L, 1, LUA_APR_POOL_MT);
  if (reference->managed)
    apr_pool_destroy(reference->pool);

  return 0;
}

/* lua_apr_pool_register() - Enable others to replace the global memory pool (e.g. mod_lua). {{{1 */

LUA_APR_EXPORT apr_pool_t *lua_apr_pool_register(lua_State *L, apr_pool_t *new_pool)
{
  return pool_register(L, new_pool, 0);
}
