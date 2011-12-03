/* Pollset module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: December 3, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * The pollset module enables [asynchronous I/O] [wp_async_io] which can
 * improve throughput, latency and/or responsiveness. It works as follows:
 *
 *  1. Create a pollset object by calling `apr.pollset()`
 *  2. Add one or more sockets to the pollset (e.g. a server socket listening
 *     for connections or a bunch of sockets receiving data)
 *  3. Call `pollset:poll()` in a loop to process readable/writable sockets
 *
 * You can keep adding and removing sockets from the pollset at runtime, just
 * keep in mind that the size given to `apr.pollset()` is a hard limit. There
 * is an example of a [simple asynchronous webserver] [async_server] that uses
 * a pollset.
 *
 * [wp_async_io]: http://en.wikipedia.org/wiki/Asynchronous_I/O
 * [async_server]: #example_asynchronous_webserver
 */

#include "lua_apr.h"
#include <apr_poll.h>

/* Internal functions. {{{1 */

typedef struct {
  lua_apr_refobj header;   /* required by new_object()                         */
  apr_pollset_t *pollset;  /* opaque pointer allocated by APR from memory pool */
  apr_pool_t *memory_pool; /* standalone memory pool for pollset               */
  apr_pollfd_t *fds;       /* file descriptor array allocated from memory pool */
  int size;                /* size of file descriptor array                    */
} lua_apr_pollset_object;

#define check_socket(L, idx) \
   check_object(L, idx, &lua_apr_socket_type)

/* check_pollset() */

static lua_apr_pollset_object* check_pollset(lua_State *L, int idx, int open) {
  lua_apr_pollset_object *object = check_object(L, idx, &lua_apr_pollset_type);
  if (open && object->pollset == NULL)
    luaL_error(L, "attempt to use a destroyed pollset");
  return object;
}

/* find_fd_by_socket() {{{2 */

static apr_pollfd_t* find_fd_by_socket(lua_apr_pollset_object *object, lua_apr_socket *socket)
{
  apr_pollfd_t *fd;
  int i;

  for (i = 0; i < object->size; i++) {
    fd = &object->fds[i];
    if (fd->desc_type == APR_POLL_SOCKET && fd->client_data == socket)
      return fd;
  }

  return NULL;
}

/* find_empty_fd() {{{2 */

static apr_pollfd_t* find_empty_fd(lua_apr_pollset_object *object)
{
  int i;

  for (i = 0; i < object->size; i++)
    if (object->fds[i].desc_type == APR_NO_DESC)
      return &object->fds[i];

  return NULL;
}

/* destroy_pollset() {{{2 */

static apr_status_t destroy_pollset(lua_apr_pollset_object *object)
{
  apr_status_t status = APR_SUCCESS;

  if (object_collectable((lua_apr_refobj*)object)) {
    if (object->pollset != NULL) {
      status = apr_pollset_destroy(object->pollset);
      object->pollset = NULL;
    }
    if (object->memory_pool != NULL) {
      apr_pool_destroy(object->memory_pool);
      object->memory_pool = NULL;
    }
  }
  release_object((lua_apr_refobj*)object);

  return status;
}

/* apr.pollset(size) -> pollset {{{1
 *
 * Create a pollset object. The number @size is the maximum number of sockets
 * that the pollset can hold. On success a pollset object is returned,
 * otherwise a nil followed by an error message is returned.
 */

int lua_apr_pollset(lua_State *L)
{
  lua_apr_pollset_object *object;
  apr_status_t status;
  int i, size;

  size = luaL_checkint(L, 1);
  object = new_object(L, &lua_apr_pollset_type);
  status = apr_pool_create(&object->memory_pool, NULL);
  if (status == APR_SUCCESS) {
    status = apr_pollset_create(&object->pollset, size, object->memory_pool, 0);
    if (status == APR_SUCCESS) {
      object->fds = apr_pcalloc(object->memory_pool, sizeof object->fds[0] * size);
      object->size = size;
      /* Unused descriptors are marked with APR_NO_DESC. */
      for (i = 0; i < size; i++)
        object->fds[i].desc_type = APR_NO_DESC;
      /* Return the new userdata. */
      return 1;
    }
    destroy_pollset(object);
  }

  return push_error_status(L, status);
}

/* pollset:add(socket, flag [, ...]) -> status {{{1
 *
 * Add a network socket to the pollset. On success true is returned, otherwise
 * a nil followed by an error message is returned. One or two of the following
 * flags should be provided:
 *
 *  - `'input'` indicates that the socket can be read without blocking
 *  - `'output'` indicates that the socket can be written without blocking
 *
 * If the socket is already in the pollset the flags of the existing entry in
 * the pollset will be combined with the new flags. If you want to *change* a
 * socket from readable to writable or the other way around, you have to first
 * remove the socket from the pollset and then add it back with the new flag.
 */

static int pollset_add(lua_State *L)
{
  const char *options[] = { "input", "output", NULL }; 
  const apr_int32_t values[] = { APR_POLLIN, APR_POLLOUT };

  lua_apr_pollset_object *object;
  apr_int16_t reqevents;
  lua_apr_socket *socket;
  apr_pollfd_t *fd;
  apr_status_t status;

  /* pollset, socket, flag1 [, flag2] */
  lua_settop(L, 4);

  /* Get the object arguments. */
  object = check_pollset(L, 1, 1);
  socket = check_socket(L, 2);

  /* Check the requested event type(s). */
  reqevents = values[luaL_checkoption(L, 3, NULL, options)];
  if (!lua_isnil(L, 4))
    reqevents |= values[luaL_checkoption(L, 4, NULL, options)];

  /* Get the private environment of the pollset. */
  object_env_private(L, 1);

  /* Check if the socket is already in the pollset. */
  fd = find_fd_by_socket(object, socket);
  if (fd != NULL) {
    /* XXX I couldn't find any documentation on having a socket that is both
     * readable and writable in the file descriptor array, and I also don't
     * know what's the intended way to change the requested events for a socket
     * that is already in the pollset. I assume that removing the socket,
     * OR'ing the flags and adding it back in will work. */
    status = APR_SUCCESS;
    /* If the flags haven't changed we don't have to do anything :-) */
    if ((fd->reqevents & reqevents) != reqevents) {
      status = apr_pollset_remove(object->pollset, fd);
      if (status == APR_SUCCESS) {
        fd->reqevents |= reqevents;
        status = apr_pollset_add(object->pollset, fd);
      }
    }
  } else {
    /* Try to add the socket to the pollset. */
    fd = find_empty_fd(object);
    if (fd == NULL) {
      status = APR_ENOMEM;
    } else {
      fd->p = socket->pool;
      fd->desc_type = APR_POLL_SOCKET;
      fd->reqevents = reqevents;
      fd->rtnevents = 0;
      fd->desc.s = socket->handle;
      fd->client_data = socket;
      /* Add the file descriptor to the pollset. */
      status = apr_pollset_add(object->pollset, fd);
      if (status == APR_SUCCESS) {
        /* Add the socket to the environment table of the pollset so that the
         * socket doesn't get garbage collected as long as it's contained in
         * the pollset. */
        lua_pushlightuserdata(L, socket);
        lua_pushvalue(L, 2);
        lua_rawset(L, 5);
      }
    }
  }

  return push_status(L, status);
}

/* pollset:remove(socket) -> status {{{1
 *
 * Remove a @socket from the pollset. On success true is returned, otherwise a
 * nil followed by an error message is returned. It is not an error if the
 * socket is not contained in the pollset.
 */

static int pollset_remove(lua_State *L)
{
  lua_apr_pollset_object *object = APR_SUCCESS;
  lua_apr_socket *socket;
  apr_pollfd_t *fd;
  apr_status_t status;

  object = check_pollset(L, 1, 1);
  socket = check_socket(L, 2);
  fd = find_fd_by_socket(object, socket);
  if (fd != NULL) {
    /* Remove it from the pollset. */
    status = apr_pollset_remove(object->pollset, fd);
    /* Remove it from our file descriptor array. */
    fd->desc_type = APR_NO_DESC;
    /* Remove it from the environment. */
    object_env_private(L, 1);
    lua_pushlightuserdata(L, socket);
    lua_pushnil(L);
    lua_rawset(L, -3);
  }

  return push_status(L, status);
}

/* pollset:poll(timeout) -> readable, writable {{{1
 *
 * Block for activity on the descriptor(s) in a pollset. The @timeout argument
 * gives the amount of time in microseconds to wait. This is a maximum, not a
 * minimum.  If a descriptor is signaled, we will wake up before this time. A
 * negative number means wait until a descriptor is signaled. On success a
 * table with sockets waiting to be read followed by a table with sockets
 * waiting to be written is returned, otherwise a nil followed by an error
 * message is returned.
 */

static int pollset_poll(lua_State *L)
{
  lua_apr_pollset_object *object;
  apr_interval_time_t timeout;
  const apr_pollfd_t *fds;
  apr_status_t status;
  apr_int32_t num_fds;
  int i;

  /* Normalize stack to (pollset, timeout, environment). */
  lua_settop(L, 2);
  object = check_pollset(L, 1, 1);
  timeout = luaL_checkint(L, 2);
  object_env_private(L, 1); /* environment @ 3 */

  /* Poll the sockets. */
  status = apr_pollset_poll(object->pollset, timeout, &num_fds, &fds);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);


  /* Create tables to hold the readable/writable sockets. */
  lua_newtable(L); /* readable @ 4 */
  lua_newtable(L); /* writable @ 5 */

  for (i = 0; i < num_fds; i++) {
    /* Find the userdata associated with the socket. */
    lua_pushlightuserdata(L, fds[i].client_data);
    lua_rawget(L, 3);
    /* Add the socket to the readable list? */
    if (fds[i].rtnevents & APR_POLLIN) {
      lua_pushvalue(L, -1);
      lua_rawseti(L, 4, lua_objlen(L, 4) + 1);
    }
    /* Add the socket to the writable list? */
    if (fds[i].rtnevents & APR_POLLOUT) {
      lua_rawseti(L, 5, lua_objlen(L, 5) + 1);
    } else
      lua_pop(L, 1);
  }

  return 2;
}

/* pollset:destroy() -> status {{{1
 *
 * Destroy a pollset. On success true is returned, otherwise a nil followed by
 * an error message is returned. Note that pollset objects are automatically
 * destroyed when they are garbage collected.
 */

static int pollset_destroy(lua_State *L)
{
  lua_apr_pollset_object *object;
  apr_status_t status;

  object = check_pollset(L, 1, 0);
  status = destroy_pollset(object);

  return push_status(L, status);
}

/* pollset:__tostring() {{{1 */

static int pollset_tostring(lua_State *L)
{
  lua_apr_pollset_object *object;

  object = check_pollset(L, 1, 0);
  if (object->pollset != NULL)
    lua_pushfstring(L, "%s (%p)", lua_apr_pollset_type.friendlyname, object->pollset);
  else
    lua_pushfstring(L, "%s (closed)", lua_apr_pollset_type.friendlyname);

  return 1;
}

/* pollset:__gc() {{{1 */

static int pollset_gc(lua_State *L)
{
  lua_apr_pollset_object *object;

  object = check_pollset(L, 1, 0);
  destroy_pollset(object);

  return 0;
}

/* Internal object definitions. {{{1 */

static luaL_reg pollset_methods[] = {
  { "add", pollset_add },
  { "remove", pollset_remove },
  { "poll", pollset_poll },
  { "destroy", pollset_destroy },
  { NULL, NULL }
};

static luaL_reg pollset_metamethods[] = {
  { "__tostring", pollset_tostring },
  { "__gc", pollset_gc },
  { NULL, NULL }
};

lua_apr_objtype lua_apr_pollset_type = {
  "lua_apr_pollset_t*",           /* metatable name in registry */
  "pollset",                      /* friendly object name       */
  sizeof(lua_apr_pollset_object), /* structure size             */
  pollset_methods,                /* methods table              */
  pollset_metamethods             /* metamethods table          */
};

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
