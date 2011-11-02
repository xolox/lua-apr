/* Pollset module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: November 2, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * TODO Short introduction goes here.
 */

#include "lua_apr.h"
#include <apr_poll.h>

typedef struct {
  lua_apr_refobj header;
  apr_pollset_t *pollset;
  apr_pool_t *memory_pool;
  apr_pollfd_t *pollfds;
  int size;
} lua_apr_pollset_object;

/* Internal functions {{{1 */

static apr_pollfd_t* find_fd(lua_apr_pollset_object *object, lua_apr_socket *socket)
{
  apr_pollfd_t *fd;
  int i;

  for (i = 0; i < object->size; i++) {
    fd = &object->pollfds[i];
    if (fd->p != NULL && fd->desc.s == socket->handle)
      return fd;
  }

  return NULL;
}

/* apr.pollset() -> pollset {{{1 */

int lua_apr_pollset(lua_State *L)
{
  lua_apr_pollset_object *object;
  apr_status_t status;
  int size;

  size = luaL_checkint(L, 1);
  object = new_object(L, &lua_apr_pollset_type);
  status = apr_pool_create(&object->memory_pool, NULL);
  if (status == APR_SUCCESS) {
    status = apr_pollset_create(&object->pollset, size, object->memory_pool, 0);
    if (status == APR_SUCCESS) {
      object->pollfds = apr_pcalloc(object->memory_pool, sizeof object->pollfds[0] * size);
      object->size = size;
      return 1;
    }
    apr_pool_destroy(object->memory_pool);
    object->memory_pool = NULL;
  }

  return push_error_status(L, status);
}

/* pollset:add(socket, flag [, ...]) -> status {{{1
 *
 * Add a network socket to the poll set. On success true is returned, otherwise
 * a nil followed by an error message is returned. One or more of the following
 * flags should be provided:
 *
 *  - `'input'` indicates that the socket can be read without blocking
 *  - `'output'` indicates that the socket can be written without blocking
 */

static int pollset_add(lua_State *L)
{
  const char *options[] = { "input", "output", NULL }; 
  const apr_int32_t values[] = { APR_POLLIN, APR_POLLOUT };

  lua_apr_pollset_object *object;
  apr_int16_t reqevents;
  lua_apr_socket *socket;
  apr_pollfd_t *pollfd;
  apr_status_t status;
  int i, envidx;

  /* Validate arguments. */
  object = check_object(L, 1, &lua_apr_pollset_type);
  socket = check_object(L, 2, &lua_apr_socket_type);

  /* Check the requested events (one or more). */
  i = 3, reqevents = 0;
  do {
    reqevents |= values[luaL_checkoption(L, i, NULL, options)];
  } while (!lua_isnoneornil(L, ++i));

  /* Get the private environment of the pollset userdata. */
  object_env_private(L, 1);
  envidx = lua_gettop(L);

  /* If the socket is already in the pollset we act like nothing happened. */
  pollfd = find_fd(object, socket);
  if (pollfd != NULL)
    return push_status(L, APR_SUCCESS);

  /* Find a free spot in the file descriptor array. */
  status = APR_ENOSPC;
  for (i = 0; i < object->size; i++) {
    pollfd = &object->pollfds[i];
    if (pollfd->p == NULL) {
      /* Fill in the details. */
      pollfd->p = socket->pool;
      pollfd->desc_type = APR_POLL_SOCKET;
      pollfd->reqevents = reqevents;
      pollfd->rtnevents = 0;
      pollfd->desc.s = socket->handle;
      /* Add the file descriptor to the pollset. */
      status = apr_pollset_add(object->pollset, pollfd);
      /* Add the socket userdata to the environment. */
      lua_pushlightuserdata(L, socket->handle);
      lua_pushvalue(L, 2);
      lua_rawset(L, envidx);
      /* We're done searching. */
      break;
    }
  }

  return push_status(L, status);
}

/* pollset:remove(socket) -> status {{{1 */

static int pollset_remove(lua_State *L)
{
  lua_apr_pollset_object *object;
  lua_apr_socket *socket;
  apr_pollfd_t *pollfd;
  apr_status_t status;

  object = check_object(L, 1, &lua_apr_pollset_type);
  pollfd = find_fd(object, socket);
  if (pollfd != NULL) {
    /* Remove it from the pollset. */
    status = apr_pollset_remove(object->pollset, pollfd);
    /* Remove it from our file descriptor array. */
    pollfd->p = NULL;
    /* Remove it from the environment. */
    object_env_private(L, 1);
    lua_pushlightuserdata(L, socket->handle);
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
  const apr_pollfd_t *pollfds;
  apr_status_t status;
  apr_int32_t num_fds;
  int i;

  /* Normalize stack to (pollset, timeout, environment). */
  lua_settop(L, 2);
  object = check_object(L, 1, &lua_apr_pollset_type);
  timeout = luaL_checkint(L, 2);
  object_env_private(L, 1); /* environment @ 3 */

  /* Poll the sockets. */
  status = apr_pollset_poll(object->pollset, timeout, &num_fds, &pollfds);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);
  
  /* Create tables to hold the readable/writable sockets. */
  lua_newtable(L); /* readable @ 4 */
  lua_newtable(L); /* writable @ 5 */

  for (i = 0; i < num_fds; i++) {
    /* Find the userdata associated with the socket. */
    lua_pushlightuserdata(L, pollfds[i].desc.s);
    lua_rawget(L, 3);
    /* Add the socket to the readable list? */
    if (pollfds[i].rtnevents & APR_POLLIN) {
      lua_pushvalue(L, -1);
      lua_rawseti(L, 4, lua_objlen(L, 4) + 1);
    }
    /* Add the socket to the writable list? */
    if (pollfds[i].rtnevents & APR_POLLOUT)
      lua_rawseti(L, 5, lua_objlen(L, 5) + 1);
    else
      lua_pop(L, 1);
  }

  return 2;
}

/* pollset:wakeup() -> status {{{1 */

static int pollset_wakeup(lua_State *L)
{
  /* TODO Implement pollset:wakeup(). */
  return 0;
}

/* pollset:destroy() -> status {{{1 */

static int pollset_destroy(lua_State *L)
{
  /* TODO Implement pollset:destroy(). */
  return 0;
}

/* pollset:__tostring() {{{1 */

static int pollset_tostring(lua_State *L)
{
  lua_apr_pollset_object *object;

  object = check_object(L, 1, &lua_apr_pollset_type);
  lua_pushfstring(L, "%s (%p)", lua_apr_pollset_type.friendlyname, object->pollset);

  return 1;
}

/* pollset:__gc() {{{1 */

static int pollset_gc(lua_State *L)
{
  lua_apr_pollset_object *object;

  object = check_object(L, 1, &lua_apr_pollset_type);
  if (object->memory_pool != NULL) {
    apr_pool_destroy(object->memory_pool);
    object->memory_pool = NULL;
  }

  return 0;
}

/* Internal object definitions. {{{1 */

static luaL_reg pollset_methods[] = {
  { "add", pollset_add },
  { "remove", pollset_remove },
  { "poll", pollset_poll },
  { "wakeup", pollset_wakeup },
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
  "pollset",                      /* friendly object name */
  sizeof(lua_apr_pollset_object), /* structure size */
  pollset_methods,                /* methods table */
  pollset_metamethods             /* metamethods table */
};

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
