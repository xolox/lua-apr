/* Process handling module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: June 16, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 */

#include "lua_apr.h"
#include <apr_thread_proc.h>
#include <apr_strings.h>
#include <apr_lib.h>

/* TODO Bind useful but missing functions in apr_proc* namespace?
 *  - apr_procattr_limit_set() isn't supported on all platforms?
 *  - apr_procattr_perms_set_register()
 *  - apr_proc_wait_all_procs()
 */

/* Structure for process objects. */
typedef struct {
  lua_apr_refobj header;
  apr_pool_t *memory_pool;
  apr_proc_t handle;
  apr_procattr_t *attr;
  const char *path;
  const char **env;
} lua_apr_proc;

/* Internal functions {{{1 */

/* proc_alloc(L, path) -- allocate and initialize process object {{{2 */

static lua_apr_proc *proc_alloc(lua_State *L, const char *path)
{
  apr_status_t status;
  lua_apr_proc *process;

  process = new_object(L, &lua_apr_proc_type);
  status = apr_pool_create(&process->memory_pool, NULL);
  if (status != APR_SUCCESS)
    process->memory_pool = NULL;
  else
    status = apr_procattr_create(&process->attr, process->memory_pool);
  if (status != APR_SUCCESS)
    raise_error_status(L, status);
  if (path != NULL)
    process->path = apr_pstrdup(process->memory_pool, path);
  return process;
}

/* proc_check(L, i) -- check for process object on Lua stack {{{2 */

static lua_apr_proc *proc_check(lua_State *L, int i)
{
  return check_object(L, i, &lua_apr_proc_type);
}

/* get_pipe(L, file, key) -- return standard input/output/error pipe {{{2 */

static int get_pipe(lua_State *L, apr_file_t *handle, const char *key)
{
  lua_apr_file *file;

  if (object_env_private(L, 1)) {
    lua_getfield(L, -1, key); /* get cached pipe object */
    if (lua_type(L, -1) == LUA_TUSERDATA)
      return 1; /* return cached pipe object */
    lua_pop(L, 1); /* pop nil result from lua_getfield() */
  }
  if (handle != NULL) {
    file = file_alloc(L, NULL, NULL);
    file->handle = handle;
    init_file_buffers(L, file, 1);
    lua_pushvalue(L, -1); /* copy reference to userdata */
    lua_setfield(L, -3, key); /* cache userdata in environment table */
    return 1; /* return new pipe */
  }
  return 0;
}

/* set_pipe(L, ck, pk, cb) -- open standard input/output/error pipe {{{2 */

static int set_pipe(lua_State *L, const char *ck, const char *pk, lua_apr_setpipe_f cb)
{
  lua_apr_proc *process;
  apr_file_t *child, *parent = NULL;

  /* Validate and collect arguments. */
  lua_settop(L, 3); /* process, child_pipe, parent_pipe */
  process = proc_check(L, 1);
  child = file_check(L, 2, 1)->handle;
  if (!lua_isnil(L, 3))
    parent = file_check(L, 3, 1)->handle;

  /* Make sure pipe(s) aren't garbage collected while process is alive! */
  object_env_private(L, 1); /* process, child_pipe, parent_pipe, environment */
  lua_insert(L, 1); /* environment, process, child_pipe, parent_pipe */
  lua_setfield(L, 1, pk); /* environment, process, child_pipe */
  lua_setfield(L, 1, ck); /* environment, process */

  return push_status(L, cb(process->attr, child, parent));
}

/* close_pipe(L, key) -- close standard input/output/error pipe {{{2 */

static void close_pipe(lua_State *L, const char *key)
{
  lua_getfield(L, 2, key);
  if (lua_type(L, -1) == LUA_TUSERDATA)
    file_close_impl(L, lua_touserdata(L, -1));
  lua_pop(L, 1);
}

/* apr.proc_create(program) -> process {{{1
 *
 * Create a child process that will execute the given @program when started.
 * Once you've called this function you still need to execute the process using
 * the `process:exec()` function. Here's a simple example that emulates Lua's
 * `os.execute()` function:
 *
 *     function execute(command)
 *       local arguments = apr.tokenize_to_argv(command)
 *       local progname = table.remove(arguments, 1)
 *       local process = apr.proc_create(progname)
 *       process:cmdtype_set('shellcmd/env')
 *       process:exec(arguments)
 *       local done, code, why = process:wait(true)
 *       return code
 *     end
 *
 *     execute 'echo This can be any process...'
 */

int lua_apr_proc_create(lua_State *L)
{
  proc_alloc(L, luaL_checkstring(L, 1));
  return 1;
}

/* apr.proc_detach(daemonize) -> status {{{1
 *
 * Detach the current process from the controlling terminal. If @daemonize
 * evaluates to true the process will [daemonize] [daemons] and become a
 * background process, otherwise it will stay in the foreground. On success
 * true is returned, otherwise a nil followed by an error message is
 * returned.
 *
 * [daemons]: http://en.wikipedia.org/wiki/Daemon_(computer_software)
 */

int lua_apr_proc_detach(lua_State *L)
{
  apr_status_t status;
  int daemonize;

  luaL_checkany(L, 1);
  daemonize = lua_toboolean(L, 1);
  status = apr_proc_detach(daemonize);

  return push_status(L, status);
}

#if APR_HAS_FORK

/* apr.proc_fork() -> process, context {{{1
 *
 * This is currently the only non-portable function in APR and by extension
 * Lua/APR. It performs a [standard UNIX fork][fork]. If the fork succeeds a
 * @process object and @context string (`'parent'` or `'child'`) are returned,
 * otherwise a nil followed by an error message is returned. The parent process
 * can use the returned @process object to wait for the child process to die:
 *
 *     if apr.proc_fork then -- forking supported?
 *       process, context = assert(apr.proc_fork())
 *       if context == 'parent' then
 *         print "Parent waiting for child.."
 *         process:wait(true)
 *         print "Parent is done!"
 *       else -- context == 'child'
 *         print "Child simulating activity.."
 *         apr.sleep(10)
 *         print "Child is done!"
 *       end
 *     end
 *
 * As the above example implies the `apr.proc_fork()` function will only be
 * defined when forking is supported on the current platform.
 *
 * [fork]: http://en.wikipedia.org/wiki/Fork_(operating_system)
 */

int lua_apr_proc_fork(lua_State *L)
{
  lua_apr_proc *process = proc_alloc(L, NULL);
  apr_status_t status = apr_proc_fork(&process->handle, process->memory_pool);
  if (status != APR_INCHILD && status != APR_INPARENT)
    return push_error_status(L, status);
  lua_pushstring(L, status == APR_INPARENT ? "parent" : "child");
  return 2;
}

#endif

/* process:addrspace_set(separate) -> status {{{1
 *
 * If @separate evaluates to true the child process will start in its own
 * address space, otherwise the child process executes in the current address
 * space from its parent. On success true is returned, otherwise a nil followed
 * by an error message is returned. The default is no on NetWare and yes on
 * other platforms.
 */

static int proc_addrspace_set(lua_State *L)
{
  apr_int32_t separate;
  apr_status_t status;
  lua_apr_proc *process;

  process = proc_check(L, 1);
  separate = lua_toboolean(L, 2);
  status = apr_procattr_addrspace_set(process->attr, separate);

  return push_status(L, status);
}

/* process:user_set(username [, password]) -> status {{{1
 *
 * Set the user under which the child process will run. On success true is
 * returned, otherwise a nil followed by an error message is returned.
 *
 * On Windows and other platforms where `apr.user_set_requires_password` is
 * true this method _requires_ a password.
 */

static int proc_user_set(lua_State *L)
{
  const char *username, *password;
  apr_status_t status;
  lua_apr_proc *process;

  process = proc_check(L, 1);
  username = luaL_checkstring(L, 2);
# if APR_PROCATTR_USER_SET_REQUIRES_PASSWORD
  password = luaL_checkstring(L, 3);
# else
  password = luaL_optstring(L, 3, NULL);
# endif
  status = apr_procattr_user_set(process->attr, username, password);

  return push_status(L, status);
}

/* process:group_set(groupname) -> status {{{1
 *
 * Set the group under which the child process will run. On success true is
 * returned, otherwise a nil followed by an error message is returned.
 */

static int proc_group_set(lua_State *L)
{
  apr_status_t status;
  const char *groupname;
  lua_apr_proc *process;

  process = proc_check(L, 1);
  groupname = luaL_checkstring(L, 2);
  status = apr_procattr_group_set(process->attr, groupname);

  return push_status(L, status);
}

/* process:cmdtype_set(type) -> status {{{1
 *
 * Set what type of command the child process will execute. On success true is
 * returned, otherwise a nil followed by an error message is returned. The
 * argument @type must be one of:
 *
 *  - `'shellcmd'`: Use the shell to invoke the program
 *  - `'shellcmd/env'`: Use the shell to invoke the program, replicating our environment
 *  - `'program'`: Invoke the program directly, without copying the environment
 *  - `'program/env'`: Invoke the program directly, replicating our environment
 *  - `'program/env/path'`: Find program in `$PATH`, replicating our environment
 */

static int proc_cmdtype_set(lua_State *L)
{
  const char *options[] = {
    "shellcmd", "shellcmd/env",
    "program", "program/env", "program/env/path",
    NULL
  };

  const apr_cmdtype_e types[] = {
    APR_SHELLCMD, APR_SHELLCMD_ENV,
    APR_PROGRAM, APR_PROGRAM_ENV, APR_PROGRAM_PATH
  };

  lua_apr_proc *process;
  apr_cmdtype_e type;
  apr_status_t status;

  process = proc_check(L, 1);
  type = types[luaL_checkoption(L, 2, NULL, options)];
  status = apr_procattr_cmdtype_set(process->attr, type);

  return push_status(L, status);
}

/* process:env_set(environment) -> status {{{1
 *
 * Set the environment variables of the child process to the key/value pairs in
 * the table @environment. On success true is returned, otherwise a nil
 * followed by an error message is returned.
 *
 * Please note that the environment table is ignored for the command types
 * `'shellcmd/env'`, `'program/env'` and `'program/env/path'` (set using the
 * `process:cmdtype_set()` method).
 */

static int proc_env_set(lua_State *L)
{
  const char *format, *message, *name, *value;
  size_t i, count;
  char **env;
  lua_apr_proc *process;

  /* validate input, calculate size of environment array */
  process = proc_check(L, 1);
  luaL_checktype(L, 2, LUA_TTABLE);
  for (count = 0, lua_pushnil(L); lua_next(L, 2); lua_pop(L, 1), count++) {
    if (lua_type(L, -2) != LUA_TSTRING) {
      format = "expected string key, got %s";
      message = lua_pushfstring(L, format, luaL_typename(L, -2));
      luaL_argerror(L, 2, message);
    }
    if (!lua_isstring(L, -1)) {
      format = "expected string value for key " LUA_QS ", got %s";
      message = lua_pushfstring(L, format, lua_tostring(L, -2), luaL_typename(L, -1));
      luaL_argerror(L, 2, message);
    }
  }

  /* convert Lua table to array of environment variables */
  env = apr_palloc(process->memory_pool, sizeof env[0] * (count+1));
  if (!env) return push_error_memory(L);
  for (i = 0, lua_pushnil(L); lua_next(L, 2); lua_pop(L, 1), i++) {
    name = lua_tostring(L, -2);
    value = lua_tostring(L, -1);
    env[i] = apr_pstrcat(process->memory_pool, name, "=", value, NULL);
    if (!env[i]) return push_error_memory(L);
  }
  env[i] = NULL;
  process->env = (const char**)env;

  lua_pushboolean(L, 1);
  return 1;
}

/* process:dir_set(path) -> status {{{1
 *
 * Set which directory the child process should start executing in. On success
 * true is returned, otherwise a nil followed by an error message is returned.
 *
 * By default child processes inherit this directory from their parent process
 * at the moment when the `process:exec()` call is made. To find out the
 * current directory see the `apr.filepath_get()` function.
 */

static int proc_dir_set(lua_State *L)
{
  apr_status_t status;
  const char *path;
  lua_apr_proc *process;

  process = proc_check(L, 1);
  path = luaL_checkstring(L, 2);
  status = apr_procattr_dir_set(process->attr, path);

  return push_status(L, status);
}

/* process:detach_set(detach) -> status {{{1
 *
 * Determine if the child should start in detached state. On success true is
 * returned, otherwise a nil followed by an error message is returned. Default
 * is no.
 */

static int proc_detach_set(lua_State *L)
{
  apr_status_t status;
  apr_int32_t detach;
  lua_apr_proc *process;

  process = proc_check(L, 1);
  detach = lua_toboolean(L, 2);
  status = apr_procattr_detach_set(process->attr, detach);

  return push_status(L, status);
}

/* process:error_check_set(enabled) -> nothing {{{1
 *
 * Specify that `process:exec()` should do whatever it can to report failures
 * directly, rather than find out in the child that something is wrong. This
 * leads to extra overhead in the calling process, but it may help you handle
 * these errors more gracefully.
 *
 * Note that this option is only useful on platforms where [fork()][fork] is
 * used.
 *
 * [fork]: http://linux.die.net/man/2/fork
 */

static int proc_error_check_set(lua_State *L)
{
  apr_status_t status;
  apr_int32_t error_check;
  lua_apr_proc *process;

  process = proc_check(L, 1);
  error_check = lua_toboolean(L, 2);
  status = apr_procattr_error_check_set(process->attr, error_check);

  return push_status(L, status);
}

/* process:io_set(stdin, stdout, stderr) -> status {{{1
 *
 * Determine if the child process should be linked to its parent through one or
 * more pipes. On success true is returned, otherwise a nil followed by an
 * error message is returned.
 *
 * Each argument gives the blocking mode of a pipe, which can be one of the
 * following strings:
 *
 *  - `'none'`: Don't create a pipe
 *  - `'full-block'`: Create a pipe that blocks until the child process writes to the pipe or dies
 *  - `'full-nonblock'`: Create a pipe that doesn't block
 *  - `'parent-block'`: Create a pipe that only blocks on the parent's end
 *  - `'child-block'`: Create a pipe that only blocks on the child's end
 *
 * *Once the child process has been started* with `process:exec()`, the pipes
 * can be accessed with the methods `process:in_get()`, `process:out_get()`
 * and `process:err_get()`.
 *
 * Here's an example that executes the external command `tr a-z A-Z` to
 * translate some characters to uppercase:
 *
 *     > p = apr.proc_create 'tr'
 *     > p:cmdtype_set('shellcmd/env')
 *     > p:io_set('child-block', 'parent-block', 'none')
 *     > p:exec{'a-z', 'A-Z'}
 *     > input = p:in_get()
 *     > output = p:out_get()
 *     > input:write('Testing, 1, 2, 3\n')
 *     > input:close()
 *     > print(output:read())
 *     TESTING, 1, 2, 3
 *     > output:close()
 *     > p:wait(true)
 */

static int proc_io_set(lua_State *L)
{
  const char *options[] = {
    "none", "full-block", "full-nonblock",
    "parent-block", "child-block", NULL
  };

  const apr_int32_t values[] = {
    APR_NO_PIPE, APR_FULL_BLOCK, APR_FULL_NONBLOCK,
    APR_PARENT_BLOCK, APR_CHILD_BLOCK
  };

  apr_status_t status;
  lua_apr_proc *process;
  apr_int32_t input, output, error;

  process = proc_check(L, 1);
  input = values[luaL_checkoption(L, 2, "none", options)];
  output = values[luaL_checkoption(L, 3, "none", options)];
  error = values[luaL_checkoption(L, 4, "none", options)];
  status = apr_procattr_io_set(process->attr, input, output, error);

  return push_status(L, status);
}

/* process:in_set(child_in [, parent_in]) -> status {{{1
 *
 * Initialize the [standard input pipe] [stdin] of the child process to an
 * existing pipe or a pair of pipes. This can be useful if you have already
 * opened a pipe (or multiple files) that you wish to use, perhaps persistently
 * across multiple process invocations - such as a log file. On success true is
 * returned, otherwise a nil followed by an error message is returned. Here's
 * a basic example that connects two processes using an anonymous pipe:
 *
 *     -- Create a gzip process to decompress the Lua source code archive.
 *     gzip = apr.proc_create 'gunzip'
 *     gzip:cmdtype_set 'shellcmd/env'
 *     gzip:in_set(apr.file_open('lua-5.1.4.tar.gz', 'rb'))
 *
 *     -- Create a tar process to list the files in the decompressed archive.
 *     tar = apr.proc_create 'tar'
 *     tar:cmdtype_set 'shellcmd/env'
 *     tar:out_set(apr.pipe_open_stdout())
 *
 *     -- Connect the two processes using an anonymous pipe.
 *     input, output = assert(apr.pipe_create())
 *     gzip:out_set(output)
 *     tar:in_set(input)
 *
 *     -- Start the pipeline by executing both processes.
 *     gzip:exec()
 *     tar:exec{'-t'}
 *
 * [stdin]: http://en.wikipedia.org/wiki/Standard_streams#Standard_input_.28stdin.29
 */

static int proc_in_set(lua_State *L)
{
  return set_pipe(L, "in_child", "in_parent",
      (lua_apr_setpipe_f)apr_procattr_child_in_set);
}

/* process:out_set(child_out [, parent_out]) -> status {{{1
 *
 * Initialize the [standard output pipe] [stdout] of the child process to an
 * existing pipe or a pair of pipes. This can be useful if you have already
 * opened a pipe (or multiple files) that you wish to use, perhaps persistently
 * across multiple process invocations - such as a log file. On success true is
 * returned, otherwise a nil followed by an error message is returned.
 *
 * [stdout]: http://en.wikipedia.org/wiki/Standard_streams#Standard_output_.28stdout.29
 */

static int proc_out_set(lua_State *L)
{
  return set_pipe(L, "out_child", "out_parent",
      (lua_apr_setpipe_f)apr_procattr_child_out_set);
}

/* process:err_set(child_err [, parent_err]) -> status {{{1
 *
 * Initialize the [standard error pipe] [stderr] of the child process to an
 * existing pipe or a pair of pipes. This can be useful if you have already
 * opened a pipe (or multiple files) that you wish to use, perhaps persistently
 * across multiple process invocations - such as a log file. On success true is
 * returned, otherwise a nil followed by an error message is returned.
 *
 * [stderr]: http://en.wikipedia.org/wiki/Standard_streams#Standard_error_.28stderr.29
 */

static int proc_err_set(lua_State *L)
{
  return set_pipe(L, "err_child", "err_parent",
      (lua_apr_setpipe_f)apr_procattr_child_err_set);
}

/* process:in_get() -> pipe {{{1
 *
 * Get the parent end of the standard input pipe (a writable pipe).
 */

static int proc_in_get(lua_State *L)
{
  lua_apr_proc *process = proc_check(L, 1);
  return get_pipe(L, process->handle.in, "in_parent");
}

/* process:out_get() -> pipe {{{1
 *
 * Get the parent end of the standard output pipe (a readable pipe).
 */

static int proc_out_get(lua_State *L)
{
  lua_apr_proc *process = proc_check(L, 1);
  return get_pipe(L, process->handle.out, "out_parent");
}

/* process:err_get() -> pipe {{{1
 *
 * Get the parent end of the standard error pipe (a readable pipe).
 */

static int proc_err_get(lua_State *L)
{
  lua_apr_proc *process = proc_check(L, 1);
  return get_pipe(L, process->handle.err, "err_parent");
}

/* process:exec([args]) -> status {{{1
 *
 * Create the child process and execute a program or shell command inside it.
 * On success true is returned, otherwise a nil followed by an error message is
 * returned. If the @args array is given the contained strings become the
 * command line arguments to the child process. The [program name] [progname]
 * for the child process defaults to the name passed into `apr.proc_create()`,
 * but you can change it by setting `args[0]`.
 *
 * [progname]: http://en.wikipedia.org/wiki/BusyBox#Single_binary
 */

static int proc_exec(lua_State *L)
{
  apr_status_t status;
  lua_apr_proc *process;
  const char *p, **args;
  int i, nargs = 0;

  lua_settop(L, 2);
  process = proc_check(L, 1);
  if (!lua_isnoneornil(L, 2)) {
    luaL_checktype(L, 2, LUA_TTABLE);
    nargs = lua_objlen(L, 2);
  }

  /* Allocate and initialize the array of arguments. */
  args = apr_palloc(process->memory_pool, sizeof args[0] * (nargs + 2));
  if (args == NULL)
    return push_error_memory(L);
  args[0] = apr_filepath_name_get(process->path);
  args[nargs+1] = NULL;

  /* Copy the arguments? */
  if (nargs > 0) {
    for (i = 0; i <= nargs; i++) {
      lua_pushinteger(L, i);
      lua_gettable(L, 2);
      p = lua_tostring(L, -1);
      if (p != NULL) /* argument */
        args[i] = apr_pstrdup(process->memory_pool, p);
      else if (i != 0) /* invalid value */
        luaL_argcheck(L, 0, 2, lua_pushfstring(L, "invalid value at index %d", i));
      lua_pop(L, 1);
    }
  }

  /* Create the child process using the given command line arguments. */
  status = apr_proc_create(&process->handle, process->path, args, process->env, process->attr, process->memory_pool);

  return push_status(L, status);
}

/* process:wait(how) -> done [, why, code] {{{1
 *
 * Wait for the child process to die. If @how is true the call blocks until the
 * process dies, otherwise the call returns immediately regardless of if the
 * process is dead or not. The first return value is false if the process isn't
 * dead yet. If it's true the process died and two more return values are
 * available. The second return value is the reason the process died, which is
 * one of:
 *
 *  - `'exit'`: Process exited normally
 *  - `'signal'`: Process exited due to a signal
 *  - `'signal/core'`: Process exited and dumped [a core file] [coredump]
 *
 * The third return value is the exit code of the process. If an error occurs a
 * nil followed by an error message is returned.
 *
 * [coredump]: http://en.wikipedia.org/wiki/Core_dump
 */

static int proc_wait(lua_State *L)
{
  apr_status_t status;
  apr_exit_why_e why;
  apr_wait_how_e how;
  lua_apr_proc *process;
  int code;

  process = proc_check(L, 1);
  how = lua_toboolean(L, 2) ? APR_WAIT : APR_NOWAIT;
  status = apr_proc_wait(&process->handle, &code, &why, how);

  if (APR_STATUS_IS_CHILD_NOTDONE(status))
    return (lua_pushboolean(L, 0), 1);
  else if (!APR_STATUS_IS_CHILD_DONE(status))
    return push_error_status(L, status);
  else
    lua_pushboolean(L, 1);

  switch (why) {
    default:
    case APR_PROC_EXIT:        lua_pushliteral(L, "exit");        break;
    case APR_PROC_SIGNAL:      lua_pushliteral(L, "signal");      break;
    case APR_PROC_SIGNAL_CORE: lua_pushliteral(L, "signal/core"); break;
  }

  lua_pushinteger(L, code);

  return 3;
}

/* process:kill(how) -> status {{{1
 *
 * Terminate a running child process. On success true is returned, otherwise a
 * nil followed by an error message is returned. The parameter @how must be one
 * of:
 *
 *  - `'never'`: The process is never sent any signals
 *  - `'always'`: The process is sent the [SIGKILL] [sigkill] signal when its
 *    Lua userdata is garbage collected
 *  - `'timeout'`: Send the [SIGTERM] [sigterm] signal, wait for 3 seconds,
 *    then send the [SIGKILL] [sigkill] signal
 *  - `'wait'`: Wait forever for the process to complete
 *  - `'once'`: Send the [SIGTERM] [sigterm] signal and then wait
 *
 * [sigkill]: http://en.wikipedia.org/wiki/SIGKILL
 * [sigterm]: http://en.wikipedia.org/wiki/SIGTERM
 */

static int proc_kill(lua_State *L)
{
  const char *options[] = {
    "never", "always", "timeout", "wait", "once", NULL,
  };

  const apr_kill_conditions_e values[] = {
    APR_KILL_NEVER, APR_KILL_ALWAYS, APR_KILL_AFTER_TIMEOUT,
    APR_JUST_WAIT, APR_KILL_ONLY_ONCE,
  };

  apr_status_t status;
  lua_apr_proc *process;
  int option;

  process = proc_check(L, 1);
  option = values[luaL_checkoption(L, 2, NULL, options)];
  status = apr_proc_kill(&process->handle, option);

  return push_status(L, status);
}

/* process:__tostring() {{{1 */

static int proc_tostring(lua_State *L)
{
  lua_apr_proc *process;

  process = proc_check(L, 1);
  lua_pushfstring(L, "%s (%p)", lua_apr_proc_type.friendlyname, process);

  return 1;
}

/* process:__gc() {{{1 */

static int proc_gc(lua_State *L)
{
  lua_apr_proc *process = proc_check(L, 1);
  if (object_collectable((lua_apr_refobj*)process)) {
    lua_settop(L, 1);
    lua_getfenv(L, 1);
    if (lua_type(L, -1) == LUA_TTABLE) {
      close_pipe(L, "in_child");
      close_pipe(L, "in_parent");
      close_pipe(L, "out_child");
      close_pipe(L, "out_parent");
      close_pipe(L, "err_child");
      close_pipe(L, "err_parent");
    }
    if (process->memory_pool != NULL) {
      apr_pool_destroy(process->memory_pool);
      process->memory_pool = NULL;
    }
  }
  release_object((lua_apr_refobj*)process);
  return 0;
}

/* }}}1 */

static luaL_Reg proc_methods[] = {
  { "cmdtype_set", proc_cmdtype_set },
  { "addrspace_set", proc_addrspace_set },
  { "detach_set" , proc_detach_set },
  { "error_check_set", proc_error_check_set },
  { "user_set", proc_user_set },
  { "group_set", proc_group_set },
  { "env_set", proc_env_set },
  { "dir_set", proc_dir_set },
  { "io_set", proc_io_set },
  { "in_get", proc_in_get },
  { "out_get", proc_out_get },
  { "err_get", proc_err_get },
  { "in_set", proc_in_set },
  { "out_set", proc_out_set },
  { "err_set", proc_err_set },
  { "exec", proc_exec },
  { "wait", proc_wait },
  { "kill", proc_kill },
  { NULL, NULL },
};

static luaL_Reg proc_metamethods[] = {
  { "__tostring", proc_tostring },
  { "__eq", objects_equal },
  { "__gc", proc_gc },
  { NULL, NULL }
};

lua_apr_objtype lua_apr_proc_type = {
  "lua_apr_proc*",      /* metatable name in registry */
  "process",            /* friendly object name */
  sizeof(lua_apr_proc), /* structure size */
  proc_methods,         /* methods table */
  proc_metamethods      /* metamethods table */
};

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
