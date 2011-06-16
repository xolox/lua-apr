/* Object model for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: June 16, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * TODO Use apr_atomic_inc() / apr_atomic_dec() !
 */

#include "lua_apr.h"

/* new_object() {{{1
 *
 * Allocate and initialize an object of the given type as a Lua userdata with
 * a metatable, returning the address of the userdata.
 */

void *new_object(lua_State *L, lua_apr_objtype *T)
{
  lua_apr_refobj *object = lua_newuserdata(L, T->objsize);
  if (object != NULL) {
    memset(object, 0, T->objsize);
    object->reference = NULL;
    object->refcount = 1;
    object->unmanaged = 0;
    init_object(L, T);
  }
  return object;
}

/* prepare_reference() {{{1
 *
 * Prepare an object for being referenced from multiple operating system
 * threads and/or Lua states by moving the object to unmanaged memory and
 * turning the original object into a reference to the object in unmanaged
 * memory. Returns the address of the object in unmanaged memory.
 */

void *prepare_reference(lua_apr_objtype *T, lua_apr_refobj *object)
{
  lua_apr_refobj *clone;
  if (object->unmanaged)
    return object;
  if (object->reference == NULL) {
    clone = malloc(T->objsize);
    if (clone == NULL)
      return NULL;
    memcpy(clone, object, T->objsize);
    object->reference = clone;
    /* just being explicit */
    clone->reference = NULL;
    clone->unmanaged = 1;
  }
  return object->reference;
}

/* proxy_object() {{{1
 *
 * Create an object of the given type which references another object. Returns
 * the address of the referenced object.
 */

void *proxy_object(lua_State *L, lua_apr_objtype *T, lua_apr_refobj *original)
{
  lua_apr_refobj *object, *reference;
  if ((object = prepare_reference(T, original)) != NULL) {
    reference = lua_newuserdata(L, sizeof(lua_apr_refobj));
    if (reference != NULL) {
      reference->reference = object->reference;
      reference->unmanaged = 0;
      init_object(L, T);
    }
  }
  return object;
}

/* init_object() {{{1
 *
 * Install a metatable and environment table on the Lua userdata at the top of
 * the Lua stack.
 */

void init_object(lua_State *L, lua_apr_objtype *T)
{
  get_metatable(L, T);
  lua_setmetatable(L, -2);
  object_env_default(L);
  lua_setfenv(L, -2);
}

/* object_collectable() {{{1
 *
 * Check whether an object can be destroyed (if the reference count is more
 * than one the object will not be destroyed, instead just the reference will
 * be garbage collected).
 */

int object_collectable(lua_apr_refobj *object)
{
  if (object->reference != NULL)
    object = object->reference;
  return object->refcount == 1;
}

/* release_object() {{{1
 *
 * Decrement the reference count of an object. When the object resides in
 * unmanaged memory and the reference count becomes zero the object will be
 * deallocated.
 */

void release_object(lua_apr_refobj *object)
{
  if (object->reference != NULL)
    object = object->reference;
  object->refcount--;
  if (object->unmanaged && object->refcount == 0)
    free(object);
}

/* object_env_default() {{{1
 *
 * Get the default environment for objects created by Lua/APR.
 */

void object_env_default(lua_State *L)
{
  const char *key = "Lua/APR default environment for userdata";

  lua_getfield(L, LUA_REGISTRYINDEX, key);
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, key);
  }
}

/* object_env_private() {{{1
 *
 * Get the private environment of an object, creating one if it doesn't exist.
 */

int object_env_private(lua_State *L, int idx)
{
  lua_getfenv(L, idx); /* get current environment table */
  object_env_default(L); /* get default environment table */
  if (!lua_equal(L, -1, -2)) {
    lua_pop(L, 1);
    return 1;
  } else {
    lua_pop(L, 2);
    lua_newtable(L); /* create environment table */
    lua_pushvalue(L, -1); /* copy reference to table */
    lua_setfenv(L, idx); /* install environment table */
    return 0;
  }
}

/* object_has_type() {{{1
 *
 * Check if the type of an object on the Lua stack matches the type defined by
 * the given Lua/APR type structure.
 */

int object_has_type(lua_State *L, int idx, lua_apr_objtype *T, int getmt)
{
  int valid = 0, top = lua_gettop(L);
  if (getmt)
    lua_getmetatable(L, idx);
  get_metatable(L, T);
  valid = lua_rawequal(L, -1, -2);
  lua_settop(L, top);
  return valid;
}

/* objects_equal() {{{1
 *
 * Check if two objects refer to the same unmanaged object.
 */

int objects_equal(lua_State *L)
{
  lua_apr_refobj *a, *b;
  /* Redundant but explicit check for type. */
  lua_getmetatable(L, 1);
  lua_getmetatable(L, 2);
  if (lua_equal(L, -1, -2)) {
    a = lua_touserdata(L, 1);
    b = lua_touserdata(L, 2);
    /* Compare the references. */
    if (a->reference != NULL && a->reference == b->reference) {
      lua_pushboolean(L, 1);
      return 1;
    }
  }
  lua_pushboolean(L, 0);
  return 1;
}

/* check_object() {{{1
 *
 * Check if the type of an object on the Lua stack matches the given type.
 */

void *check_object(lua_State *L, int idx, lua_apr_objtype *T)
{
  if (object_has_type(L, idx, T, 1)) {
    /* This is basically all that's needed to enable sharing userdata
     * objects between operating system threads and Lua states.. :-) */
    lua_apr_refobj *object = lua_touserdata(L, idx);
    if (object->reference != NULL)
      object = object->reference;
    return object;
  }
  luaL_typerror(L, idx, T->typename);
  return NULL;
}

/* get_metatable() {{{1
 *
 * Get the metatable for the given type, creating it if it doesn't exist.
 */

int get_metatable(lua_State *L, lua_apr_objtype *T)
{
  luaL_getmetatable(L, T->typename);
  if (lua_type(L, -1) != LUA_TTABLE) {
    lua_pop(L, 1);
    luaL_newmetatable(L, T->typename);
    luaL_register(L, NULL, T->metamethods);
    if (T->methods != NULL) {
      lua_newtable(L);
      luaL_register(L, NULL, T->methods);
      lua_setfield(L, -2, "__index");
    }
  }
  return 1;
}

/* object_incref() {{{1
 *
 * Increment the reference count of an object. Returns the address of the
 * referenced object.
 */

void *object_incref(lua_apr_refobj *object)
{
  if (object->reference != NULL)
    object = object->reference;
  object->refcount++;
  return object;
}

/* object_decref() {{{1
 *
 * Decrement the reference count on an object.
 * This does not destroy the object in any way!
 */

int object_decref(void *ptr)
{
  lua_apr_refobj *object;

  object = ptr;
  object->refcount--;

  return object->refcount == 0;
}

/* refpool_alloc() {{{1
 *
 * Allocate a reference counted APR memory pool.
 */

lua_apr_pool *refpool_alloc(lua_State *L)
{
  apr_status_t status;
  apr_pool_t *memory_pool;
  lua_apr_pool *refpool = NULL;

  status = apr_pool_create(&memory_pool, NULL);
  if (status != APR_SUCCESS)
    raise_error_status(L, status);

  refpool = apr_palloc(memory_pool, sizeof(*refpool));
  refpool->ptr = memory_pool;
  refpool->refs = 0;

  return refpool;
}

/* refpool_incref() {{{1
 *
 * Increase the reference count of an APR memory pool.
 */

apr_pool_t* refpool_incref(lua_apr_pool *refpool)
{
  refpool->refs++;
  return refpool->ptr;
}

/* refpool_decref() {{{1
 *
 * Decrease the reference count of an APR memory pool.
 */

void refpool_decref(lua_apr_pool *refpool)
{
  refpool->refs--;
  if (refpool->refs == 0)
    apr_pool_destroy(refpool->ptr);
}

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
