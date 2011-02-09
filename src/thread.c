/* Multi threading module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: February 9, 2011
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

/* Heads up: I can't find any documentation on this but apr_thread_exit()
 * destroys the thread's memory pool (at least on UNIX it does). This makes
 * sense but complicates the allocation of "lua_apr_thread" structures: they
 * can't be allocated as Lua userdata (because the child thread might outlive
 * its creator) nor can they be allocated from the memory pool (because
 * apr_thread_exit() destroys the memory pool and the thread structure with it,
 * even though the creator might still want to access the thread structure). */

/* Private parts. {{{1 */

const char *status_names[] = { "init", "running", "done", "error" };

#define thread_busy(T) \
  ((T)->status == TS_INIT || (T)->status == TS_RUNNING)

typedef struct {
  int refcount;
  apr_pool_t *pool;
  apr_thread_t *handle;
  apr_threadattr_t *attr;
  lua_State *state;
  struct threadbuf {
    const char *data;
    size_t size;
  } function, argument, result;
  enum { TS_INIT, TS_RUNNING, TS_DONE, TS_ERROR } status;
  int detached, joined;
} lua_apr_thread;

/* check_thread(L, idx, check_joinable) {{{2 */

static lua_apr_thread *check_thread(lua_State *L, int idx, int check_joinable)
{
  lua_apr_thread *object, **indirect;

  indirect = check_object(L, idx, &lua_apr_thread_type);
  object = *indirect;
  if (check_joinable && object->detached)
    luaL_error(L, "attempt to join a detached thread");

  return object;
}

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

/* thread_destroy(thread) {{{2 */

static void thread_destroy(lua_apr_thread *thread)
{
  /* The current thread is no longer using the thread structure. */
  thread->refcount--;

  /* XXX If the thread exited with an error and the user didn't check the
   * result using thread:join() we print the error message to stderr. */
  if (thread->refcount == 0 && thread->status == TS_ERROR && !thread->joined)
    fprintf(stderr, "Lua/APR thread exited with error: %.*s\n", thread->result.size, thread->result.data);

  /* XXX Only destroy the thread when the child has finished executing and
   * neither Lua state will reference the thread structure anymore. */
  if (!thread_busy(thread) && thread->refcount == 0) {
    if (thread->state != NULL) {
      lua_close(thread->state);
      thread->state = NULL;
    }
    free(thread);
  }
}

/* thread_runner(handle, thread) {{{2 */

static void *thread_runner(apr_thread_t *handle, lua_apr_thread *thread)
{
  lua_State *L;
  const char *data;
  size_t size;
  int lstatus;

  /* The child thread is now using the thread structure. */
  thread->refcount++;

  /* Create the child Lua state. */
  L = thread->state = luaL_newstate();
  if (thread->state == NULL) {
    const char *msg = error_message_memory;
    thread->result.data = msg;
    thread->result.size = strlen(msg);
    thread->status = TS_ERROR;
  } else {

    /* Load the standard library. */
    luaL_openlibs(L);

    /* Push the code and argument strings and evaluate the code. */
    lua_settop(L, 0);
    lua_pushcfunction(L, error_handler);
    lstatus = luaL_loadbuffer(L, thread->function.data, thread->function.size, thread->function.data);
    if (lstatus == 0) {
      lua_pushlstring(L, thread->argument.data, thread->argument.size);
      thread->status = TS_RUNNING;
      lstatus = lua_pcall(L, 1, 1, 1);
    }

    /* Save the returned string / error message. */
    data = lua_tolstring(L, -1, &size);
    thread->result.size = size;
    thread->result.data = apr_pmemdup(thread->pool, data, (apr_size_t) size);

    /* Update the status as the last modification of the thread structure. */
    thread->status = lstatus ? TS_ERROR : TS_DONE;
  }

  /* Terminate (and destroy) the thread. */
  thread_destroy(thread);
  apr_thread_exit(handle, APR_SUCCESS);

  return NULL;
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
  lua_apr_thread *thread = NULL;
  apr_status_t status;
  const char *fun, *arg;
  size_t funsize, argsize;
  int i;

  /* Set the Lua stack to its expected size. */
  lua_settop(L, 2);

  /* Validate and copy arguments. */
  fun = luaL_checklstring(L, 1, &funsize);
  arg = luaL_optlstring(L, 2, "", &argsize);

  /* Create the Lua/APR thread object. */
  thread = new_object_ex(L, &lua_apr_thread_type, 1);
  if (thread == NULL)
    goto fail;
  thread->refcount = 1;
  thread->status = TS_INIT;

  /* Create a memory pool for the thread structures and string buffers. */
  status = apr_pool_create(&thread->pool, NULL);
  if (status != APR_SUCCESS)
    goto fail;

  /* Move both string arguments to the thread's memory pool. */
  thread->function.size = funsize;
  thread->function.data = apr_pmemdup(thread->pool, fun, (apr_size_t) funsize);
  thread->argument.size = argsize;
  thread->argument.data = apr_pmemdup(thread->pool, arg, (apr_size_t) argsize);

  /* Start the operating system thread. */
  status = apr_threadattr_create(&thread->attr, thread->pool);
  if (status == APR_SUCCESS)
    status = apr_thread_create(&thread->handle, thread->attr,
        (apr_thread_start_t)thread_runner, thread, thread->pool);
  if (status != APR_SUCCESS)
    goto fail;

  /* XXX Wait for the child thread to start before returning control to Lua:
   * I've found that when the parent Lua state is closed while the child thread
   * is still being initialized one can experience hard to debug crashes. */
  while (thread->status == TS_INIT) {
    apr_sleep(APR_USEC_PER_SEC / 20);
    if (thread->status != TS_INIT)
      break;
    else if (i++ % 20 == 0)
      /* Let the user know what's happening in case this turns out to take a
       * while, but don't bother the user if we only need to wait once.. */
      LUA_APR_DBG("Lua/APR waiting for thread to initialize .. (%i)", i);
  }

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

  object = check_thread(L, 1, 1);

  /* Only block when the thread hasn't finished yet. */
  if (thread_busy(object)) {
    status = apr_thread_join(&unused, object->handle);
    if (status != APR_SUCCESS)
      return push_error_status(L, status);
  }

  /* Remember that the user has checked the result. */
  object->joined = 1;

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
  thread_destroy(check_thread(L, 1, 0));
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
