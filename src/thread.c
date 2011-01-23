/* Multi threading module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: January 23, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * This is an experimental multi threading module that makes it possible to
 * execute Lua functions in dedicated [Lua states] [Lua_state] and [operating
 * system threads] [threading]. The current implementation is very basic which
 * is by design; I'm working on a more convenient interface that serializes
 * arguments and return values on top of this module.
 *
 * [Lua_state]: http://www.lua.org/manual/5.1/manual.html#lua_State
 * [threading]: http://en.wikipedia.org/wiki/Thread_%28computer_science%29
 */

#include "lua_apr.h"
#include <lualib.h>
#include <apr_strings.h>

#if APR_HAS_THREADS

/* Private parts. {{{1 */

const char *status_names[] = { "init", "running", "done", "error" };

#define check_thread(L, idx) \
  check_object((L), (idx), &lua_apr_thread_type)

typedef struct {
  apr_pool_t *pool;
  apr_thread_t *handle;
  apr_threadattr_t *attr;
  apr_status_t exit_status;
  lua_State *state;
  struct threadbuf {
    const char *data;
    size_t size;
  } function, argument, result;
  enum { TS_INIT, TS_RUNNING, TS_DONE, TS_ERROR } status;
} lua_apr_thread;

/* error_handler(state) {{{2 */

/* Copied from lua-5.1.4/src/lua.c where it's called traceback() */

static int error_handler(lua_State *L) {
  if (!lua_isstring(L, 1))  /* 'message' not a string? */
    return 1;  /* keep it intact */
  lua_getfield(L, LUA_GLOBALSINDEX, "debug");
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    return 1;
  }
  lua_getfield(L, -1, "traceback");
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2);
    return 1;
  }
  lua_pushvalue(L, 1);  /* pass error message */
  lua_pushinteger(L, 2);  /* skip this function and traceback */
  lua_call(L, 2, 1);  /* call debug.traceback */
  return 1;
}

/* thread_runner(handle, thread) {{{2 */

static void *thread_runner(apr_thread_t *handle, lua_apr_thread *thread)
{
  lua_State *L = thread->state;
  const char *data;
  size_t size;

  /* Load the standard library. */
  luaL_openlibs(L);

  /* Push the code and argument strings and evaluate the code. */
  lua_settop(L, 0);
  lua_pushcfunction(L, error_handler);
  if (luaL_loadbuffer(L, thread->function.data, thread->function.size, "apr.thread_runner()")) {
    LUA_APR_DBG("Error: %s", lua_tostring(L, -1));
    thread->status = TS_ERROR;
  } else {
    lua_pushlstring(L, thread->argument.data, thread->argument.size);
    thread->status = TS_RUNNING;
    if (lua_pcall(L, 1, 1, 1)) {
      LUA_APR_DBG("Error: %s", lua_tostring(L, -1));
      thread->status = TS_ERROR;
    } else {
      thread->status = TS_DONE;
    }
  }

  /* Save the returned string or error message. */
  data = lua_tolstring(L, -1, &size);
  thread->result.size = size;
  thread->result.data = apr_pmemdup(thread->pool, data, (apr_size_t) size);

  /* Terminate the OS thread. */
  thread->exit_status = apr_thread_exit(handle, APR_SUCCESS);

  return NULL;
}

/* thread_destroy(thread) {{{2 */

static void thread_destroy(lua_apr_thread *thread)
{
  /* TODO Should the thread be forcefully closed here?! */
  if (thread != NULL) {
    if (thread->state != NULL) {
      lua_close(thread->state);
      thread->state = NULL;
    }
    if (thread->pool != NULL) {
      apr_pool_destroy(thread->pool);
      thread->pool = NULL;
    }
  }
}

/* apr.thread_create(code [, arg]) -> thread {{{1
 *
 * Execute the Lua code in the string argument @code in a dedicated Lua state
 * and *operating system* thread. The optional string @arg is passed as the
 * first and only argument to the Lua code (where it can be accessed with the
 * expression `...`). On success a thread object is returned, otherwise a nil
 * followed by an error message is returned. You can use `thread:join()` to
 * wait for the thread to finish and get its return value.
 */

int lua_apr_thread_create(lua_State *L)
{
  apr_status_t status = APR_ENOMEM;
  lua_apr_thread *thread;
  const char *data;
  size_t size;

  /* Expecting string of code and optional string argument. */
  lua_settop(L, 2);

  /* Create the Lua/APR thread object.
   * FIXME Don't rely on parent Lua state staying alive for userdata allocation?! */
  thread = new_object(L, &lua_apr_thread_type);
  if (thread == NULL)
    goto fail;
  /* Redundant but explicit initialization. */
  thread->status = TS_INIT;
  thread->exit_status = APR_SUCCESS;

  /* Create a memory pool for the thread structure and string buffers. */
  status = apr_pool_create(&thread->pool, NULL);
  if (status != APR_SUCCESS)
    goto fail;

  /* Move both string arguments to the thread's memory pool. */
  data = luaL_checklstring(L, 1, &size);
  thread->function.size = size;
  thread->function.data = apr_pmemdup(thread->pool, data, (apr_size_t) size);
  data = luaL_optlstring(L, 2, "", &size);
  thread->argument.size = size;
  thread->argument.data = apr_pmemdup(thread->pool, data, (apr_size_t) size);

  /* Create an independent Lua state. */
  thread->state = luaL_newstate();
  if (thread->state == NULL)
    goto fail;

  /* Start the operating system thread. */
  status = apr_threadattr_create(&thread->attr, thread->pool);
  if (status == APR_SUCCESS)
    status = apr_thread_create(&thread->handle, thread->attr,
        (apr_thread_start_t)thread_runner, thread, thread->pool);
  if (status != APR_SUCCESS)
    goto fail;

  /* Return the thread object. */
  return 1;

fail:
  thread_destroy(thread);
  return push_error_status(L, status);
}

/* apr.thread_yield() -> nothing {{{1
 *
 * Force the current thread to yield the processor. This causes the currently
 * executing thread to temporarily pause and allow other threads to execute.
 */

int lua_apr_thread_yield(lua_State *L)
{
  apr_thread_yield();
  return 0;
}

/* thread:detach() -> status {{{1
 *
 * Detach a thread to make the operating system automatically reclaim storage
 * for the thread once it terminates. On success true is returned, otherwise
 * nil followed by an error message is returned.
 */

static int thread_detach(lua_State *L)
{
  lua_apr_thread *object;
  apr_status_t status;

  object = check_thread(L, 1);
  status = apr_thread_detach(object->handle);
  return push_status(L, status);
}

/* thread:join() -> status, result {{{1
 *
 * Block until a thread stops executing and return its result. If the thread
 * terminated with an error a nil followed by an error message is returned,
 * otherwise true followed by result of the thread is returned. The result of
 * the thread is the string returned by the code that was passed to
 * `apr.thread_create()`. If the code didn't return a string the result will be
 * an empty string.
 */

static int thread_join(lua_State *L)
{
  lua_apr_thread *object;
  apr_status_t status, unused;

  object = check_thread(L, 1);

  /* Don't block when the thread has already finished. */
  if (object->status == TS_INIT || object->status == TS_RUNNING) {
    status = apr_thread_join(&unused, object->handle);
    if (status != APR_SUCCESS)
      return push_error_status(L, status);
  }

  /* Return error status in parent if possible. */
  if (object->exit_status != APR_SUCCESS)
    return push_error_status(L, object->exit_status);

  /* Push the thread's exit status and return value. */
  if (object->status == TS_DONE)
    lua_pushboolean(L, 1);
  else
    lua_pushnil(L);
  lua_pushlstring(L, object->result.data, object->result.size);
  return 2;
}

/* thread:status() -> status {{{1
 *
 * Returns a string describing the state of the thread:
 *
 *  - `'init'`: the thread is being initialized
 *  - `'running'`: the thread is currently running
 *  - `'done'`: the thread terminated successfully
 *  - `'error'`: the thread encountered an error
 */

static int thread_status(lua_State *L)
{
  lua_apr_thread *object;

  object = check_thread(L, 1);
  lua_pushstring(L, status_names[object->status]);
  return 1;
}

/* thread:__tostring() {{{1 */

static int thread_tostring(lua_State *L)
{
  lua_apr_thread *object = check_thread(L, 1);
  lua_pushfstring(L, "thread (%s)", status_names[object->status]);
  return 1;
}

/* thread:__gc() {{{1 */

static int thread_gc(lua_State *L)
{
  lua_apr_thread *object = check_thread(L, 1);
  thread_destroy(object);
  return 0;
}

/* Lua/APR thread object metadata {{{1 */

static luaL_Reg thread_methods[] = {
  { "detach", thread_detach },
  { "join", thread_join },
  { "status", thread_status },
  { NULL, NULL }
};

static luaL_Reg thread_metamethods[] = {
  { "__gc", thread_gc },
  { "__tostring", thread_tostring },
  { NULL, NULL }
};

lua_apr_objtype lua_apr_thread_type = {
  "lua_apr_thread*",
  "thread",
  sizeof(lua_apr_thread),
  thread_methods,
  thread_metamethods
};

#endif
