/* Multi threading module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: June 30, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * This is an experimental multi threading module that makes it possible to
 * execute Lua functions in dedicated [Lua states] [Lua_state] and [operating
 * system threads] [threading]. When you create a thread you can pass it any
 * number of arguments and when a thread exits it can return any number of
 * return values. In both cases the following types are valid:
 *
 *  - nil
 *  - boolean
 *  - number
 *  - string (binary safe)
 *  - any userdata object created by the Lua/APR binding
 *
 * Please consider the following issues when using this module:
 *
 *  - When you pass a userdata object to another thread you shouldn't use it
 *    from the original thread after that, because the Lua/APR binding doesn't
 *    protect object access with a thread safe lock. This will probably be
 *    fixed in the near future (hey, I said it was experimental)
 *
 *  - When you start a thread and let it get garbage collected without having
 *    called `thread:join()`, the thread will be joined for you (because
 *    failing to do so while the main thread is terminating can crash the
 *    process)
 *
 * [Lua_state]: http://www.lua.org/manual/5.1/manual.html#lua_State
 * [threading]: http://en.wikipedia.org/wiki/Thread_%28computer_science%29
 * [libc]: http://en.wikipedia.org/wiki/C_standard_library
 */

#include "lua_apr.h"
#include <lualib.h>
#include <apr_strings.h>
#include <apr_portable.h>

#if APR_HAS_THREADS

/* Private parts. {{{1 */

const char *status_names[] = { "init", "running", "done", "error" };

#define check_thread(L, idx) \
  (lua_apr_thread*)check_object(L, idx, &lua_apr_thread_type)

#define thread_busy(T) \
  ((T)->status == TS_INIT || (T)->status == TS_RUNNING)

typedef enum { TS_INIT, TS_RUNNING, TS_DONE, TS_ERROR } thread_status_t;

typedef struct {
  lua_apr_refobj header;
  apr_pool_t *pool;
  apr_thread_t *handle;
  apr_threadattr_t *attr;
  void *input, *output;
  volatile thread_status_t status;
  int joined;
} lua_apr_thread;

/* error_handler(state) {{{2 */

/* Copied from lua-5.1.4/src/lua.c where it's called traceback() */

static int error_handler(lua_State *L)
{
  if (!lua_isstring(L, 1)) /* 'message' not a string? */
    return 1; /* keep it intact */
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
  lua_pushvalue(L, 1); /* pass error message */
  lua_pushinteger(L, 2); /* skip this function and traceback */
  lua_call(L, 2, 1); /* call debug.traceback */
  return 1;
}

/* thread_destroy(thread) {{{2 */

static void thread_destroy(lua_apr_thread *thread)
{
  if (object_collectable((lua_apr_refobj*)thread)) {
    free(thread->input);
    free(thread->output);
  }
  release_object((lua_apr_refobj*)thread);
}

/* thread_runner(handle, thread) {{{2 */

static void* lua_apr_cc thread_runner(apr_thread_t *handle, lua_apr_thread *thread)
{
  const char *function;
  size_t length;
  lua_State *L;
  int status;

  /* The child thread is now using the thread structure. */
  object_incref((lua_apr_refobj*)thread);

  /* Start by creating a new Lua state. */
  if ((L = luaL_newstate()) == NULL) {
    status = TS_ERROR;
    thread->output = strdup("Failed to create Lua state");
  } else {
    luaL_openlibs(L); /* load standard library */
    lua_settop(L, 0); /* normalize stack */
    lua_pushcfunction(L, error_handler); /* push error handler */
    push_tuple(L, thread->input); /* push thread arguments */
    function = lua_tolstring(L, 2, &length); /* compile chunk */
    if (luaL_loadbuffer(L, function, length, function)) {
      thread->output = strdup(lua_tostring(L, -1));
      status = TS_ERROR;
    } else {
      lua_replace(L, 2); /* replace string with function */
      thread->status = TS_RUNNING;
      if (lua_pcall(L, lua_gettop(L) - 2, LUA_MULTRET, 1)) {
        thread->output = strdup(lua_tostring(L, -1));
        status = TS_ERROR;
      } else {
        check_tuple(L, 2, lua_gettop(L), &thread->output);
        status = TS_DONE;
      }
    }
    lua_close(L);
  }

  thread->status = status;
  thread_destroy(thread);
  apr_thread_exit(handle, APR_SUCCESS);

  /* To make the compiler happy. */
  return NULL;
}

/* apr.thread_create(code [, ...]) -> thread {{{1
 *
 * Execute the Lua code in the string argument @code in a dedicated Lua state
 * and operating system thread. Any extra arguments are passed onto the Lua
 * code (where they can be accessed with the expression `...`). On success a
 * thread object is returned, otherwise a nil followed by an error message is
 * returned. You can use `thread:join()` to wait for the thread to finish and
 * get its return values.
 *
 * *This function is binary safe.*
 */

int lua_apr_thread_create(lua_State *L)
{
  lua_apr_thread *thread = NULL;
  apr_status_t status = APR_ENOMEM;
  int top = lua_gettop(L);

  luaL_checktype(L, 1, LUA_TSTRING);

  /* Create the Lua/APR thread object. */
  thread = new_object(L, &lua_apr_thread_type);
  if (thread == NULL)
    goto fail;
  thread->status = TS_INIT;

  /* Create memory pool for thread (freed by apr_thread_exit()). */
  status = apr_pool_create(&thread->pool, NULL);
  if (status != APR_SUCCESS)
    goto fail;

  /* Save the string of code and any other arguments.
   * TODO Dump functions here when supported?! */
  if (!check_tuple(L, 1, top, &thread->input))
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
  if (thread != NULL)
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

/* thread:join() -> status [, result, ...] {{{1
 *
 * Block until a thread stops executing and return its result. If the thread
 * terminated with an error a nil followed by an error message is returned,
 * otherwise true is returned, followed by any return values of the thread
 * function.
 *
 * *This function is binary safe.*
 */

static int thread_join(lua_State *L)
{
  lua_apr_thread *object;
  apr_status_t status, unused;

  object = check_thread(L, 1);
  lua_settop(L, 1);

  /* Don't join more than once. */
  if (!object->joined) {
    status = apr_thread_join(&unused, object->handle);
    if (status != APR_SUCCESS)
      return push_error_status(L, status);
    object->joined = 1;
  }

  /* Push the status and any results. */
  if (object->status == TS_DONE) {
    lua_pushboolean(L, 1);
    push_tuple(L, object->output);
  } else {
    lua_pushboolean(L, 0);
    lua_pushstring(L, object->output);
  }

  return lua_gettop(L) - 1;
}

/* thread:status() -> status {{{1
 *
 * Returns a string describing the state of the thread:
 *
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
  lua_pushfstring(L, "%s (%s)",
      lua_apr_thread_type.friendlyname,
      status_names[object->status]);
  return 1;
}

/* thread:__gc() {{{1 */

static int thread_gc(lua_State *L)
{
  lua_apr_thread *thread;
  apr_status_t status, unused;

  thread = check_thread(L, 1);
  if (!thread->joined) {
    fprintf(stderr, "Lua/APR joining child thread from __gc() hook ..\n");
    status = apr_thread_join(&unused, thread->handle);
    if (status != APR_SUCCESS) {
      char message[LUA_APR_MSGSIZE];
      apr_strerror(status, message, count(message));
      fprintf(stderr, "Lua/APR failed to join thread: %s\n", message);
    } else if (thread->status == TS_ERROR) {
      fprintf(stderr, "Lua/APR thread exited with error: %s\n", (char*)thread->output);
    }
  }
  thread_destroy(thread);

  return 0;
}

/* Lua/APR thread object metadata {{{1 */

static luaL_Reg thread_methods[] = {
  { "join", thread_join },
  { "status", thread_status },
  { NULL, NULL }
};

static luaL_Reg thread_metamethods[] = {
  { "__tostring", thread_tostring },
  { "__eq", objects_equal },
  { "__gc", thread_gc },
  { NULL, NULL }
};

lua_apr_objtype lua_apr_thread_type = {
  "lua_apr_thread*",      /* metatable name in registry */
  "thread",               /* friendly object name */
  sizeof(lua_apr_thread), /* structure size */
  thread_methods,         /* methods table */
  thread_metamethods      /* metamethods table */
};

#endif
