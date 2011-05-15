/* Multi threading module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: May 15, 2011
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
 *  - When a thread object is garbage collected while the thread is still
 *    running, the Lua/APR binding will join the thread using an internal mutex
 *    and condition variable from inside the garbage collection handler. This
 *    is to avoid that the parent Lua state and the operating system process
 *    exit while the child is still running, because this can lead to hard to
 *    debug segmentation faults deep inside [libc] [libc]
 *
 * [Lua_state]: http://www.lua.org/manual/5.1/manual.html#lua_State
 * [threading]: http://en.wikipedia.org/wiki/Thread_%28computer_science%29
 * [libc]: http://en.wikipedia.org/wiki/C_standard_library
 */

#include "lua_apr.h"
#include <lualib.h>
#include <apr_strings.h>
#include <apr_portable.h>
#include <apr_thread_cond.h>

#if APR_HAS_THREADS

/* I've left the debugging messages in for now in case the mutex and condition
 * variable handling turns out to be buggy after publishing this code (you'll
 * have to define LUA_APR_DEBUG_THREADS=1 before anything is printed). */

#ifndef LUA_APR_DEBUG_THREADS
#define LUA_APR_DEBUG_THREADS 0
#endif

#define LUA_APR_THREAD_MSG(msg, ...) \
  if (LUA_APR_DEBUG_THREADS) \
    LUA_APR_DBG(msg, __VA_ARGS__)

/* The thread which initialized the Lua/APR binding. */
static apr_os_thread_t initial_thread;

/* Private parts. {{{1 */

const char *status_names[] = { "init", "running", "done", "error" };

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
  int detached, joined;
  apr_os_thread_t parent;
} lua_apr_thread;

/* check_thread(L, idx, check_joinable) {{{2 */

static lua_apr_thread *check_thread(lua_State *L, int idx, int check_joinable)
{
  lua_apr_thread *object = check_object(L, idx, &lua_apr_thread_type);
  if (check_joinable && object->detached)
    luaL_error(L, "attempt to join a detached thread");
  return object;
}

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

static void thread_destroy(lua_State *L, lua_apr_thread *thread)
{
  if (object_collectable((lua_apr_refobj*)thread)) {
    /* If the thread exited with an error and the user didn't check the
     * result using thread:join() we print the error message to stderr. */
    free(thread->input);
    free(thread->output);
  }
  release_object(L, (lua_apr_refobj*)thread);
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
      lua_replace(L, 2);
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

  thread_destroy(NULL, thread);
  threads_decrement(L);

  /* If an error occurred, make sure the user sees it. */
  if (status == TS_ERROR)
    fprintf(stderr, "Lua/APR thread exited with error: %s\n", (char*)thread->output);

  thread->status = status;
  /* XXX Theoretically, if the parent thread inspects thread->status at this
   * point, the child thread hasn't terminated yet. If the parent thread were
   * to terminate before the child thread, a crash would most likely result...
   * I don't like this one bit but can't see how there's any around it!
   *
   * (I've debugged several crash bugs caused by race conditions between parent
   * and child threads before publishing this code and wound up running the
   * thread_queue.lua tests for a 1000 iterations before I decided to publish
   * this code). */
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
  thread = prepare_reference(L, &lua_apr_thread_type, (lua_apr_refobj*)thread);
  if (thread == NULL)
    goto fail;
  thread->status = TS_INIT;
  thread->parent = apr_os_thread_current();

  /* Create a memory pool for the thread structure. */
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

  /* Remember that we started another thread. */
  threads_increment(L);

  /* Return the thread object. */
  return 1;

fail:
  if (thread != NULL)
    thread_destroy(L, thread);
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

  object = check_thread(L, 1, 0);
  status = apr_thread_detach(object->handle);
  if (status == APR_SUCCESS)
    object->detached = 1;
  return push_status(L, status);
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

  object = check_thread(L, 1, 1);
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

  object = check_thread(L, 1, 0);
  lua_pushstring(L, status_names[object->status]);

  return 1;
}

/* thread:__tostring() {{{1 */

static int thread_tostring(lua_State *L)
{
  lua_apr_thread *object = check_thread(L, 1, 0);
  lua_pushfstring(L, "%s (%s)",
      lua_apr_thread_type.friendlyname,
      status_names[object->status]);
  return 1;
}

/* thread:__gc() {{{1 */

static int thread_gc(lua_State *L)
{
  thread_destroy(L, check_thread(L, 1, 0));
  return 0;
}

/* Module initialization and finalization. {{{1 */

static apr_pool_t *pool = NULL;
static apr_thread_mutex_t *mutex;
static apr_thread_cond_t *condition;
static volatile int num_threads = 0;

void threads_initialize(lua_State *L)
{
  apr_status_t status;

  LUA_APR_THREAD_MSG("(L=%p) Initializing threading module!", L);

  /* XXX Remember which thread initialized the Lua/APR binding, because this
   * thread will have to "join" any child threads before it exits, otherwise
   * nasty crashes can result. I say "join" because we actually use a mutex
   * and condition variable instead of apr_thread_join() so that detached
   * threads are also supported (though I'm not sure how useful this is). */
  initial_thread = apr_os_thread_current();

  status = apr_pool_create(&pool, NULL);
  if (status == APR_SUCCESS)
    apr_thread_mutex_create(&mutex, APR_THREAD_MUTEX_UNNESTED, pool);
  if (status == APR_SUCCESS)
    apr_thread_cond_create(&condition, pool);
  if (status != APR_SUCCESS)
    raise_error_status(L, status);
}

int threads_error(lua_State *L, apr_status_t status)
{
  if (status == APR_SUCCESS)
    return 0;
  if (L != NULL) {
    raise_error_status(L, status);
  } else {
    char message[LUA_APR_MSGSIZE];
    apr_strerror(status, message, count(message));
    fprintf(stderr, "Lua/APR threading module internal error: %s\n", message);
  }
  return 1;
}

void threads_lock(lua_State *L)
{
  LUA_APR_THREAD_MSG("(L=%p) Enabling threading module lock!", L);
  threads_error(L, apr_thread_mutex_lock(mutex));
}

void threads_unlock(lua_State *L, apr_status_t status)
{
  LUA_APR_THREAD_MSG("(L=%p) Disabling threading module lock!", L);
  if (status == APR_SUCCESS)
    status = apr_thread_mutex_unlock(mutex);
  else
    apr_thread_mutex_unlock(mutex);
  if (L == NULL)
      apr_pool_destroy(pool);
  threads_error(L, status);
}

void threads_increment(lua_State *L)
{
  threads_lock(L);
  LUA_APR_THREAD_MSG("(L=%p) Raising child thread count from %i to %i", L, num_threads, num_threads + 1);
  num_threads++;
  threads_unlock(L, APR_SUCCESS);
}

void threads_decrement(lua_State *L)
{
  threads_lock(L);
  LUA_APR_THREAD_MSG("(L=%p) Lowering child thread count from %i to %i", L, num_threads, num_threads - 1);
  num_threads--;
  LUA_APR_THREAD_MSG("(L=%p) Signaling child thread termination", L);
  threads_unlock(L, apr_thread_cond_signal(condition));
}

void threads_terminate()
{
  if (apr_os_thread_equal(initial_thread, apr_os_thread_current())) {
    threads_lock(NULL);
    LUA_APR_THREAD_MSG("%s", "Terminating threading module!");
    while (num_threads > 0) {
      LUA_APR_THREAD_MSG("%s", "Waiting for child thread termination signal..");
      threads_error(NULL, apr_thread_cond_wait(condition, mutex));
    }
    LUA_APR_THREAD_MSG("%s", "Looks like all child threads finished?!");
    threads_unlock(NULL, APR_SUCCESS);
  }
}

/* Lua/APR thread object metadata {{{1 */

static luaL_Reg thread_methods[] = {
  { "detach", thread_detach },
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
