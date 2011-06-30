/* Object model for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: June 30, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 */

#include "lua_apr.h"

static lua_apr_refobj *root_object(lua_apr_refobj *object)
{
  while (object->reference != NULL)
    object = object->reference;
  return object;
}

/* new_object() {{{1
 *
 * Allocate and initialize an object of the given type as a Lua userdata with
 * a metatable, returning the address of the userdata.
 */

void *new_object(lua_State *L, lua_apr_objtype *T)
{
  lua_apr_refobj *object = lua_newuserdata(L, T->objsize);
  memset(object, 0, T->objsize);
  object->reference = NULL;
  object->refcount = 1;
  object->unmanaged = 0;
  init_object(L, T);
  return object;
}

/* prepare_reference() {{{1
 *
 * Prepare an object for being referenced from multiple operating system
 * threads and/or Lua states by moving the object to unmanaged memory and
 * turning the original object into a reference to the object in unmanaged
 * memory. Returns the address of the object in unmanaged memory.
 *
 * XXX Note that this does not increment the reference count!
 */

void *prepare_reference(lua_apr_objtype *T, lua_apr_refobj *object)
{
  object = root_object(object);
  if (object->reference == NULL && !object->unmanaged) {
    lua_apr_refobj *clone = malloc(T->objsize);
    if (clone == NULL)
      return NULL;
    memcpy(clone, object, T->objsize);
    apr_atomic_set32(&clone->refcount, 1);
    clone->unmanaged = 1;
    object->reference = clone;
  }
  return object;
}

/* create_reference() {{{1
 *
 * Create an object of the given type which references another object. Returns
 * the address of the referenced object.
 *
 * XXX Note that this does not increment the reference count!
 */

void create_reference(lua_State *L, lua_apr_objtype *T, lua_apr_refobj *original)
{
  lua_apr_refobj *reference;
  /* Objects are never moved back from unmanaged to managed memory so we don't
   * need to allocate a full structure here; we only need the fields defined in
   * the lua_apr_refobj structure. */
  reference = lua_newuserdata(L, sizeof *reference);
  reference->reference = root_object(original);
  reference->unmanaged = 0;
  init_object(L, T);
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
 * than one the object should not be destroyed, instead just the reference
 * should be garbage collected).
 *
 * XXX Even though object_incref() and object_decref() use atomic operations
 * this function can break the atomicity; it promises something it doesn't
 * control. Whether this can be a problem in practice, I don't know...
 */

int object_collectable(lua_apr_refobj *object)
{
  object = root_object(object);
  return apr_atomic_read32(&object->refcount) == 1;
}

/* release_object() {{{1
 *
 * Decrement the reference count of an object. When the object resides in
 * unmanaged memory and the reference count becomes zero the object will be
 * deallocated.
 */

void release_object(lua_apr_refobj *object)
{
  object = root_object(object);
  if (object_decref(object) && object->unmanaged)
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
 * Check if two objects refer to the same unmanaged object. This is an
 * implementation of the __eq metamethod for Lua/APR userdata objects.
 */

int objects_equal(lua_State *L)
{
  lua_apr_refobj *a, *b;

  /* Get and compare the metatables. */
  lua_getmetatable(L, 1);
  lua_getmetatable(L, 2);
  if (lua_equal(L, -1, -2)) {
    /* Compare the referenced objects. */
    a = root_object(lua_touserdata(L, 1));
    b = root_object(lua_touserdata(L, 2));
    lua_pushboolean(L, a == b);
  } else
    lua_pushboolean(L, 0);
  return 1;
}

/* check_object() {{{1
 *
 * Check if the type of a userdata object on the Lua stack matches the given
 * Lua/APR type and return a pointer to the userdata object.
 */

void *check_object(lua_State *L, int idx, lua_apr_objtype *T)
{
  if (!object_has_type(L, idx, T, 1))
    luaL_typerror(L, idx, T->typename);
  return root_object(lua_touserdata(L, idx));
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
 * Increment the reference count of an object.
 */

void object_incref(lua_apr_refobj *object)
{
  object = root_object(object);
  apr_atomic_inc32(&object->refcount);
}

/* object_decref() {{{1
 *
 * Decrement the reference count of an object.
 * This does not destroy the object in any way!
 */

int object_decref(lua_apr_refobj *object)
{
  object = root_object(object);
  return apr_atomic_dec32(&object->refcount) == 0;
}

/* refpool_alloc() {{{1
 *
 * Allocate a reference counted APR memory pool that can be shared between
 * multiple Lua/APR objects in the same Lua state and OS thread (that is to say
 * no synchronization or atomic instructions are). The memory pool is destroyed
 * when the last reference is released.
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
