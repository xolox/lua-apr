/* Thread queues module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: June 19, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * The valid types that can be transported through thread queues are documented
 * in the [multi threading](#multi_threading) module.
 */

#include "lua_apr.h"
#if APR_HAS_THREADS

#define check_queue(L, idx) \
  ((lua_apr_queue*)check_object((L), (idx), &lua_apr_queue_type))

/* Structure for thread queue objects. */
typedef struct {
  lua_apr_refobj header;
  apr_pool_t *pool;
  apr_queue_t *handle;
} lua_apr_queue;

/* Internal functions. {{{1 */

static void close_queue_real(lua_apr_queue *object)
{
  if (object_collectable((lua_apr_refobj*)object)) {
    if (object->pool != NULL) {
      apr_pool_destroy(object->pool);
      object->pool = NULL;
    }
  }
  release_object((lua_apr_refobj*)object);
}

typedef apr_status_t (lua_apr_cc *apr_queue_push_func)(apr_queue_t*, void*);

static int queue_push_real(lua_State *L, apr_queue_push_func cb)
{
  void *tuple;
  apr_status_t status = APR_ENOMEM;
  lua_apr_queue *object = check_queue(L, 1);
  if (check_tuple(L, 2, lua_gettop(L), &tuple))
    status = cb(object->handle, tuple);
  return push_status(L, status);
}

typedef apr_status_t (lua_apr_cc *apr_queue_pop_func)(apr_queue_t*, void**);

static int queue_pop_real(lua_State *L, apr_queue_pop_func cb)
{
  void *tuple;
  int n;
  lua_apr_queue *object = check_queue(L, 1);
  apr_status_t status = cb(object->handle, &tuple);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);
  n = push_tuple(L, tuple);
  free(tuple);
  return n;
}

/* apr.thread_queue([capacity]) -> queue {{{1
 *
 * Create a [FIFO queue] [fifo]. The optional argument @capacity controls the
 * maximum size of the queue and defaults to 1. On success the queue object is
 * returned, otherwise a nil followed by an error message is returned.
 *
 * [fifo]: http://en.wikipedia.org/wiki/FIFO
 */

int lua_apr_thread_queue(lua_State *L)
{
  apr_status_t status;
  lua_apr_queue *object;
  unsigned int capacity;

  capacity = luaL_optlong(L, 1, 1);
  luaL_argcheck(L, capacity >= 1, 1, "capacity must be >= 1");
  object = new_object(L, &lua_apr_queue_type);
  status = apr_pool_create(&object->pool, NULL);
  if (status == APR_SUCCESS)
    status = apr_queue_create(&object->handle, capacity, object->pool);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);

  return 1;
}

/* queue:push(value [, ...]) -> status {{{1
 *
 * Add a tuple of one or more values to the queue. This call will block if the
 * queue is full. On success true is returned, otherwise a nil followed by an
 * error message and error code is returned:
 *
 *  - `'EINTR'`: the blocking was interrupted (try again)
 *  - `'EOF'`: the queue has been terminated
 *
 * *This function is binary safe.*
 */

static int queue_push(lua_State *L)
{
  return queue_push_real(L, apr_queue_push);
}

/* queue:pop() -> value [, ...]  {{{1
 *
 * Get an object from the queue. This call will block if the queue is empty. On
 * success true is returned, otherwise a nil followed by an error message and
 * error code is returned:
 *
 *  - `'EINTR'`: the blocking was interrupted (try again)
 *  - `'EOF'`: the queue has been terminated
 *
 * *This function is binary safe.*
 */

static int queue_pop(lua_State *L)
{
  return queue_pop_real(L, apr_queue_pop);
}

/* queue:trypush(value [, ...]) -> status {{{1
 *
 * Add a tuple of one or more values to the queue. This call doesn't block if
 * the queue is full. On success true is returned, otherwise a nil followed by
 * an error message and error code is returned:
 *
 *  - `'EINTR'`: the blocking was interrupted (try again)
 *  - `'EAGAIN'`: the queue is full
 *  - `'EOF'`: the queue has been terminated
 *
 * *This function is binary safe.*
 */

static int queue_trypush(lua_State *L)
{
  return queue_push_real(L, apr_queue_trypush);
}

/* queue:trypop() -> value [, ...] {{{1
 *
 * Get an object from the queue. This call doesn't block if the queue is empty.
 * On success true is returned, otherwise a nil followed by an error message
 * and error code is returned:
 *
 *  - `'EINTR'`: the blocking was interrupted (try again)
 *  - `'EAGAIN'`: the queue is empty
 *  - `'EOF'`: the queue has been terminated
 *
 * *This function is binary safe.*
 */

static int queue_trypop(lua_State *L)
{
  return queue_pop_real(L, apr_queue_trypop);
}

/* queue:interrupt() -> status {{{1
 *
 * Interrupt all the threads blocking on this queue. On success true is
 * returned, otherwise a nil followed by an error message is returned.
 */

static int queue_interrupt(lua_State *L)
{
  apr_status_t status;
  lua_apr_queue *object;
  
  object = check_queue(L, 1);
  status = apr_queue_interrupt_all(object->handle);

  return push_status(L, status);
}

/* queue:terminate() -> status {{{1
 *
 * Terminate the queue, sending an interrupt to all the blocking threads. On
 * success true is returned, otherwise a nil followed by an error message is
 * returned.
 */

static int queue_terminate(lua_State *L)
{
  apr_status_t status;
  lua_apr_queue *object;
  
  object = check_queue(L, 1);
  status = apr_queue_term(object->handle);

  return push_status(L, status);
}

/* queue:close() -> status {{{1
 *
 * Close the handle @queue and (if no other threads are using the queue)
 * destroy the queue and release the associated memory. This function always
 * returns true (it cannot fail).
 *
 * This will be done automatically when the @queue object is garbage collected
 * which means you don't need to call this unless you want to reclaim memory as
 * soon as possible.
 */

static int queue_close(lua_State *L)
{
  close_queue_real(check_queue(L, 1));
  lua_pushboolean(L, 1);
  return 1;
}

/* queue:__tostring() {{{1 */

static int queue_tostring(lua_State *L)
{
  lua_pushfstring(L, "%s (%p)",
      lua_apr_queue_type.friendlyname, check_queue(L, 1));
  return 1;
}

/* queue:__gc() {{{1 */

static int queue_gc(lua_State *L)
{
  close_queue_real(check_queue(L, 1));
  return 0;
}

/* }}}1 */

static luaL_Reg queue_methods[] = {
  { "push", queue_push },
  { "pop", queue_pop },
  { "trypush", queue_trypush },
  { "trypop", queue_trypop },
  { "interrupt", queue_interrupt },
  { "terminate", queue_terminate },
  { "close", queue_close },
  { NULL, NULL }
};

static luaL_Reg queue_metamethods[] = {
  { "__tostring", queue_tostring },
  { "__eq", objects_equal },
  { "__gc", queue_gc },
  { NULL, NULL }
};

lua_apr_objtype lua_apr_queue_type = {
  "lua_apr_queue*",      /* metatable name in registry */
  "thread queue",        /* friendly object name */
  sizeof(lua_apr_queue), /* structure size */
  queue_methods,         /* methods table */
  queue_metamethods      /* metamethods table */
};

#endif
