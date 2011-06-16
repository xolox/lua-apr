/* Shared memory module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: June 16, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * [Shared memory] [shm] is memory that may be simultaneously accessed by
 * multiple programs with an intent to provide communication among them. The
 * Lua/APR binding represents shared memory as file objects through the
 * `shm:read()`, `shm:write()` and `shm:seek()` methods.
 *
 * [shm]: http://en.wikipedia.org/wiki/Shared_memory
 */

#include "lua_apr.h"
#include <apr_shm.h>

typedef struct {
  lua_apr_refobj header;
  apr_pool_t *pool;
  apr_shm_t *handle;
  void *base;
  apr_size_t size;
  lua_apr_readbuf input;
  lua_apr_writebuf output;
  lua_apr_buffer *last_op;
} lua_apr_shm;

/* Internal functions. {{{1 */

static void init_shm(lua_State *L, lua_apr_shm *object)
{
  object->base = apr_shm_baseaddr_get(object->handle);
  object->size = apr_shm_size_get(object->handle) - LUA_APR_BUFSLACK;
  object->last_op = &object->input.buffer;
  init_unmanaged_buffers(L,
      &object->input, &object->output,
      object->base, object->size);
}

static lua_apr_shm* check_shm(lua_State *L, int idx)
{
  return check_object(L, idx, &lua_apr_shm_type);
}

static apr_status_t shm_destroy_real(lua_apr_shm *object)
{
  apr_status_t status = APR_SUCCESS;
  if (object_collectable((lua_apr_refobj*)object)) {
    if (object->handle != NULL) {
      status = apr_shm_destroy(object->handle);
      apr_pool_destroy(object->pool);
      object->handle = NULL;
    }
  }
  release_object((lua_apr_refobj*)object);
  return status;
}

/* apr.shm_create(filename, size) -> shm object {{{1
 *
 * Create and make accessible a shared memory segment. The @filename argument
 * is the file to use for shared memory on platforms that require it. The @size
 * argument is the desired size of the segment.
 *
 * A note about anonymous vs. named shared memory segments: Not all platforms
 * support anonymous shared memory segments, but in some cases it is preferred
 * over other types of shared memory implementations. Passing a nil @filename
 * parameter to this function will cause the subsystem to use anonymous shared
 * memory segments. If such a system is not available, the error `'ENOTIMPL'`
 * is returned as the third return value (the first and second being nil and an
 * error message string).
 */

int lua_apr_shm_create(lua_State *L)
{
  apr_status_t status;
  lua_apr_shm *object;
  const char *filename;
  apr_size_t reqsize;

  filename = lua_isnil(L, 1) ? NULL : luaL_checkstring(L, 1);

  /* XXX Add some bytes to reqsize so that we can mark the end of the shared
   * memory segment with NUL. This is required by the buffered I/O interface
   * whose name is a bit of a misnomer since I introduced shared memory :-) */
  reqsize = luaL_checkinteger(L, 2) + LUA_APR_BUFSLACK;

  object = new_object(L, &lua_apr_shm_type);
  if (object == NULL)
    return push_error_memory(L);
  status = apr_pool_create(&object->pool, NULL);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);
  status = apr_shm_create(&object->handle, reqsize, filename, object->pool);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);
  init_shm(L, object);

  return 1;
}

/* apr.shm_attach(filename) -> shm object {{{1
 *
 * Attach to a shared memory segment that was created by another process. The
 * @filename argument is the file used to create the original segment (this
 * must match the original filename). On success a shared memory object is
 * returned, otherwise a nil followed by an error message is returned.
 */

int lua_apr_shm_attach(lua_State *L)
{
  apr_status_t status;
  lua_apr_shm *object;
  const char *filename;

  filename = luaL_checkstring(L, 1);

  object = new_object(L, &lua_apr_shm_type);
  status = apr_pool_create(&object->pool, NULL);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);
  status = apr_shm_attach(&object->handle, filename, object->pool);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);
  init_shm(L, object);

  return 1;
}

/* apr.shm_remove(filename) -> status {{{1
 *
 * Remove the named resource associated with a shared memory segment,
 * preventing attachments to the resource, but not destroying it. On success
 * true is returned, otherwise a nil followed by an error message is
 * returned.
 *
 * This function is only supported on platforms which support name-based shared
 * memory segments, and will return the error code `'ENOTIMPL'` on platforms
 * without such support. Removing the file while the shared memory is in use is
 * not entirely portable, caller may use this to enhance obscurity of the
 * resource, but be prepared for the the call to fail, and for concurrent
 * attempts to create a resource of the same name to also fail.
 *
 * Note that the named resource is also removed when a shared memory object
 * created by `apr.shm_create()` is garbage collected.
 */

int lua_apr_shm_remove(lua_State *L)
{
  apr_status_t status;
  const char *filename;

  filename = luaL_checkstring(L, 1);
  status = apr_shm_remove(filename, to_pool(L));

  return push_status(L, status);
}

/* shm:read([format, ...]) -> mixed value, ... {{{1
 *
 * This function implements the interface of Lua's `file:read()` function.
 */

static int shm_read(lua_State *L)
{
  lua_apr_shm *object = check_shm(L, 1);
  object->last_op = &object->input.buffer;
  return read_buffer(L, &object->input);
}

/* shm:write(value [, ...]) -> status {{{1
 *
 * This function implements the interface of Lua's `file:write()` function.
 */

static int shm_write(lua_State *L)
{
  lua_apr_shm *object = check_shm(L, 1);
  object->last_op = &object->output.buffer;
  return write_buffer(L, &object->output);
}

/* shm:seek([whence [, offset]]) -> offset {{{1
 *
 * This function implements the interface of Lua's `file:seek()` function.
 */

static int shm_seek(lua_State *L)
{
  const char *const modes[] = { "set", "cur", "end", NULL };
  lua_apr_shm *object;
  size_t offset;
  int mode;

  object = check_shm(L, 1);
  mode = luaL_checkoption(L, 2, "cur", modes);
  offset = luaL_optlong(L, 3, 0);

  if (mode == 1) { /* CUR */
    offset += object->last_op->index;
  } else if (mode == 2) { /* END */
    offset += object->last_op->limit - 1;
  }

  luaL_argcheck(L, offset >= 0, 2, "cannot seek before start of shared memory segment!");
  luaL_argcheck(L, offset < object->size, 2, "cannot seek past end of shared memory segment!");
  object->input.buffer.index = offset;
  object->output.buffer.index = offset;
  lua_pushinteger(L, offset);

  return 1;
}

/* shm:detach() -> status {{{1
 *
 * Detach from a shared memory segment without destroying it. On success true
 * is returned, otherwise a nil followed by an error message is returned.
 */

static int shm_detach(lua_State *L)
{
  apr_status_t status;
  lua_apr_shm *object;

  object = check_shm(L, 1);
  status = apr_shm_detach(object->handle);

  return push_status(L, status);
}

/* shm:destroy() -> status {{{1
 *
 * Destroy a shared memory segment and associated memory. On success true is
 * returned, otherwise a nil followed by an error message is returned. Note
 * that this will be done automatically when the shared memory object is
 * garbage collected and has not already been destroyed.
 */

static int shm_destroy(lua_State *L)
{
  lua_apr_shm *object;
  apr_status_t status;

  object = check_shm(L, 1);
  status = shm_destroy_real(object);

  return push_status(L, status);
}

/* shm:__tostring() {{{1 */

static int shm_tostring(lua_State *L)
{
  lua_apr_shm *object;

  object = check_shm(L, 1);
  if (object->handle != NULL)
    lua_pushfstring(L, "%s (%p)", lua_apr_shm_type.friendlyname, object);
  else
    lua_pushfstring(L, "%s (closed)", lua_apr_shm_type.friendlyname);

  return 1;
}

/* shm:__gc() {{{1 */

static int shm_gc(lua_State *L)
{
  shm_destroy_real(check_shm(L, 1));
  return 0;
}

/* }}}1 */

static luaL_reg shm_metamethods[] = {
  { "__tostring", shm_tostring },
  { "__eq", objects_equal },
  { "__gc", shm_gc },
  { NULL, NULL }
};

static luaL_reg shm_methods[] = {
  { "read", shm_read },
  { "write", shm_write },
  { "seek", shm_seek },
  { "detach", shm_detach },
  { "destroy", shm_destroy },
  { NULL, NULL }
};

lua_apr_objtype lua_apr_shm_type = {
  "lua_apr_shm*",
  "shared memory",
  sizeof(lua_apr_shm),
  shm_methods,
  shm_metamethods
};
