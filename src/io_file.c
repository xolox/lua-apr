/* File I/O handling module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: June 16, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 */

#include "lua_apr.h"
#include <apr_strings.h>
#include <apr_file_io.h>
#include <apr_file_info.h>
#include <apr_lib.h>
#include <stdio.h>

static int push_file_status(lua_State*, lua_apr_file*, apr_status_t);
static int push_file_error(lua_State*, lua_apr_file*, apr_status_t);

/* TODO Bind functions missing from io_file.c
 *  - apr_file_pipe_create_ex()
 *  - apr_file_sync()
 *  - apr_file_datasync()
 */

#if APR_MAJOR_VERSION > 1 || (APR_MAJOR_VERSION == 1 && APR_MINOR_VERSION >= 4)

/* apr.file_link(source, target) -> status {{{1
 *
 * Create a [hard link] [hard_link] to the specified file. On success true is
 * returned, otherwise a nil followed by an error message is returned. Both
 * files must reside on the same device.
 *
 * Please note that this function will only be available when the Lua/APR
 * binding is compiled against APR 1.4 or newer because the [apr\_file\_link()]
 * [apr_file_link] function wasn't available in earlier releases.
 *
 * [hard_link]: http://en.wikipedia.org/wiki/Hard_link
 * [apr_file_link]: http://apr.apache.org/docs/apr/trunk/group__apr__file__io.html#ga7911275c0e97edc064b8167be658f9e
 */

int lua_apr_file_link(lua_State *L)
{
  const char *source, *target;
  apr_status_t status;

  source = luaL_checkstring(L, 1);
  target = luaL_checkstring(L, 2);
  status = apr_file_link(source, target);

  return push_status(L, status);
}

#endif

/* apr.file_copy(source, target [, permissions]) -> status {{{1
 *
 * Copy the file @source to @target. On success true is returned, otherwise a
 * nil followed by an error message is returned. The @permissions argument is
 * documented elsewhere. The new file does not need to exist, it will be
 * created if required. If the new file already exists, its contents will be
 * overwritten.
 */

int lua_apr_file_copy(lua_State *L)
{
  const char *source, *target;
  apr_fileperms_t permissions;
  apr_status_t status;

  source = luaL_checkstring(L, 1);
  target = luaL_checkstring(L, 2);
  permissions = check_permissions(L, 3, 1);
  status = apr_file_copy(source, target, permissions, to_pool(L));

  return push_status(L, status);
}

/* apr.file_append(source, target [, permissions]) -> status {{{1
 *
 * Append the file @source to @target. On success true is returned, otherwise a
 * nil followed by an error message is returned. The @permissions argument is
 * documented elsewhere. The new file does not need to exist, it will be
 * created if required.
 */

int lua_apr_file_append(lua_State *L)
{
  const char *source, *target;
  apr_fileperms_t permissions;
  apr_status_t status;

  source = luaL_checkstring(L, 1);
  target = luaL_checkstring(L, 2);
  permissions = check_permissions(L, 3, 1);
  status = apr_file_append(source, target, permissions, to_pool(L));

  return push_status(L, status);
}

/* apr.file_rename(source, target) -> status {{{1
 *
 * Rename the file @source to @target. On success true is returned, otherwise a
 * nil followed by an error message is returned. If a file exists at the new
 * location, then it will be overwritten. Moving files or directories across
 * devices may not be possible.
 */

int lua_apr_file_rename(lua_State *L)
{
  const char *source, *target;
  apr_status_t status;

  source = luaL_checkstring(L, 1);
  target = luaL_checkstring(L, 2);
  status = apr_file_rename(source, target, to_pool(L));

  return push_status(L, status);
}

/* apr.file_remove(path) -> status {{{1
 *
 * Delete the file pointed to by @path. On success true is returned, otherwise
 * a nil followed by an error message is returned. If the file is open, it
 * won't be removed until all instances of the file are closed.
 */

int lua_apr_file_remove(lua_State *L)
{
  apr_status_t status;
  const char *path;

  path = luaL_checkstring(L, 1);
  status = apr_file_remove(path, to_pool(L));

  return push_status(L, status);
}

/* apr.file_mtime_set(path, mtime) -> status {{{1
 *
 * Set the last modified time of the file pointed to by @path to @mtime. On
 * success true is returned, otherwise a nil followed by an error message is
 * returned.
 */

int lua_apr_file_mtime_set(lua_State *L)
{
  apr_status_t status;
  const char *path;
  apr_time_t mtime;

  path = luaL_checkstring(L, 1);
  mtime = time_check(L, 2);
  status = apr_file_mtime_set(path, mtime, to_pool(L));

  return push_status(L, status);
}

/* apr.file_attrs_set(path, attributes) -> status {{{1
 *
 * Set the attributes of the file pointed to by @path. On success true is
 * returned, otherwise a nil followed by an error message is returned.
 *
 * The table @attributes should consist of string keys and boolean values. The
 * supported attributes are `readonly`, `hidden` and `executable`.
 *
 * This function should be used in preference to explicit manipulation of the
 * file permissions, because the operations to provide these attributes are
 * platform specific and may involve more than simply setting permission bits.
 */

int lua_apr_file_attrs_set(lua_State *L)
{
  apr_fileattrs_t attributes, valid;
  const char *path, *key;
  apr_status_t status;

  path = luaL_checkstring(L, 1);
  luaL_checktype(L, 2, LUA_TTABLE);

  attributes = valid = 0;
  lua_pushnil(L);
  while (lua_next(L, 2)) {
    key = lua_tostring(L, -2);
    if (strcmp(key, "readonly") == 0) {
      valid |= APR_FILE_ATTR_READONLY;
      if (lua_toboolean(L, -1))
        attributes |= APR_FILE_ATTR_READONLY;
    } else if (strcmp(key, "hidden") == 0) {
      valid |= APR_FILE_ATTR_HIDDEN;
      if (lua_toboolean(L, -1))
        attributes |= APR_FILE_ATTR_HIDDEN;
    } else if (strcmp(key, "executable") == 0) {
      valid |= APR_FILE_ATTR_EXECUTABLE;
      if (lua_toboolean(L, -1))
        attributes |= APR_FILE_ATTR_EXECUTABLE;
    } else {
      luaL_argerror(L, 2, lua_pushfstring(L, "invalid key " LUA_QS, key));
    }
    lua_pop(L, 1);
  }
  status = apr_file_attrs_set(path, attributes, valid, to_pool(L));

  return push_status(L, status);
}

/* apr.file_perms_set(path, permissions) -> status {{{1
 *
 * Set the permissions of the file specified by @path. On success true is
 * returned, otherwise a nil followed by an error message is returned.
 *
 * Warning: Some platforms may not be able to apply all of the available
 * permission bits, in this case a third value `'INCOMPLETE'` is returned (see
 * error handling).
 */

int lua_apr_file_perms_set(lua_State *L)
{
  const char *path;
  apr_status_t status;
  apr_fileperms_t perms;

  path = luaL_checkstring(L, 1);
  perms = check_permissions(L, 2, 0);
  status = apr_file_perms_set(path, perms);

  return push_status(L, status);
}

/* apr.stat(path [, property, ...]) -> value, ... {{{1
 *
 * Get the status of the file pointed to by @path. On success, if no properties
 * are given a table of property name/value pairs is returned, otherwise the
 * named properties are returned in the same order as the arguments. On failure
 * a nil followed by an error message is returned.
 *
 * The following fields are supported:
 *
 *  - `name` is a string containing the name of the file in proper case
 *  - `path` is a string containing the absolute pathname of the file
 *  - `type` is one of the strings `'directory'`, `'file'`, `'link'`, `'pipe'`,
 *    `'socket'`, `'block device'`, `'character device'` or `'unknown'`
 *  - `user` is a string containing the name of user that owns the file
 *  - `group` is a string containing the name of the group that owns the file
 *  - `size` is a number containing the size of the file in bytes
 *  - `csize` is a number containing the storage size consumed by the file
 *  - `ctime` is the time the file was created, or the inode was last changed
 *  - `atime` is the time the file was last accessed
 *  - `mtime` is the time the file was last modified
 *  - `nlink` is the number of hard links to the file
 *  - `inode` is a unique number within the file system on which the file
 *    resides
 *  - `dev` is a number identifying the device on which the file is stored
 *  - `protection` is a 9 character string with the file system @permissions
 *  - `link` *is a special flag that does not return a field*, instead it is
 *    used to signal that symbolic links should not be followed, i.e. the
 *    status of the link itself should be returned
 *
 * Here are some examples:
 *
 *     > -- Here's an example of a table with all properties:
 *     > = apr.stat('lua-5.1.4.tar.gz')
 *     {
 *      name = 'lua-5.1.4.tar.gz',
 *      path = 'lua-5.1.4.tar.gz',
 *      type = 'file',
 *      user = 'peter',
 *      group = 'peter',
 *      size = 216679,
 *      csize = 217088,
 *      ctime = 1284159662.7264,
 *      atime = 1287954158.6019,
 *      mtime = 1279317348.194,
 *      nlink = 1,
 *      inode = 1838576,
 *      dev  = 64514,
 *      protection = 'rw-r--r--',
 *     }
 *     > -- To check whether a directory exists:
 *     > function isdir(p) return apr.stat(p, 'type') == 'directory' end
 *     > = isdir('.')
 *     true
 *     > -- To get a file's size in bytes:
 *     > function filesize(p) return apr.stat(p, 'size') end
 *     > = filesize('lua-5.1.4.tar.gz')
 *     216679
 */

int lua_apr_stat(lua_State *L)
{
  const char *path, *name, *dir;
  apr_pool_t *memory_pool;
  lua_apr_stat_context context = { 0 };
  apr_status_t status;

  memory_pool = to_pool(L);
  path = luaL_checkstring(L, 1);
  name = apr_filepath_name_get(path);
  dir = apr_pstrndup(memory_pool, path, name - path);
  context.firstarg = 2;
  context.lastarg = lua_gettop(L);
  check_stat_request(L, &context);
  status = apr_stat(&context.info, path, context.wanted, memory_pool);
  if (status != APR_SUCCESS && !APR_STATUS_IS_INCOMPLETE(status))
    return push_error_status(L, status);

  /* XXX apr_stat() doesn't fill in finfo.name (tested on Linux) */
  if (!(context.info.valid & APR_FINFO_NAME)) {
    context.info.valid |= APR_FINFO_NAME;
    context.info.name = name;
  }

  return push_stat_results(L, &context, dir);
}

/* apr.file_open(path [, mode [, permissions]]) -> file {{{1
 *
 * <em>This function imitates Lua's `io.open()` function, so here is the
 * documentation for that function:</em>
 *
 * This function opens a file, in the mode specified in the string @mode. It
 * returns a new file handle, or, in case of errors, nil plus an error
 * message. The @mode string can be any of the following:
 *
 *  - `'r'`: read mode (the default)
 *  - `'w'`: write mode
 *  - `'a'`: append mode
 *  - `'r+'`: update mode, all previous data is preserved
 *  - `'w+'`: update mode, all previous data is erased
 *  - `'a+'`: append update mode, previous data is preserved, writing is only
 *    allowed at the end of file
 *
 * The @mode string may also have a `b` at the end, which is needed in some
 * systems to open the file in binary mode. This string is exactly what is used
 * in the standard C function [fopen()] [fopen]. The @permissions argument is
 * documented elsewhere.
 *
 * [fopen]: http://linux.die.net/man/3/fopen
 */

int lua_apr_file_open(lua_State *L)
{
  apr_status_t status;
  lua_apr_file *file;
  apr_fileperms_t perm;
  apr_int32_t flags;
  const char *path;
  char *mode;

  path = luaL_checkstring(L, 1);
  mode = (char *)luaL_optstring(L, 2, "r");
  perm = check_permissions(L, 3, 0);
  flags = 0;

  /* Parse the mode string.
   * TODO: Verify that these mappings are correct! */
  if (*mode == 'r') {
    flags |= APR_FOPEN_READ, mode++;
    if (*mode == '+') flags |= APR_FOPEN_WRITE, mode++;
    if (*mode == 'b') flags |= APR_FOPEN_BINARY, mode++;
    if (*mode == '+') flags |= APR_FOPEN_WRITE;
  } else if (*mode == 'w') {
    flags |= APR_FOPEN_WRITE | APR_FOPEN_CREATE | APR_FOPEN_TRUNCATE, mode++;
    if (*mode == '+') flags |= APR_FOPEN_READ, mode++;
    if (*mode == 'b') flags |= APR_FOPEN_BINARY, mode++;
    if (*mode == '+') flags |= APR_FOPEN_READ;
  } else if (*mode == 'a') {
    flags |= APR_FOPEN_WRITE | APR_FOPEN_CREATE | APR_FOPEN_APPEND, mode++;
    if (*mode == '+') flags |= APR_FOPEN_READ, mode++;
    if (*mode == 'b') flags |= APR_FOPEN_BINARY, mode++;
    if (*mode == '+') flags |= APR_FOPEN_READ;
  }

  /* Default to read mode just like Lua. */
  if (!(flags & APR_FOPEN_WRITE))
    flags |= APR_FOPEN_READ;

  /* Create the file object and open the file. */
  file = file_alloc(L, path, NULL);
  status = apr_file_open(&file->handle, path, flags, perm, file->pool->ptr);
  if (status != APR_SUCCESS)
    return push_file_error(L, file, status);
  init_file_buffers(L, file, !(flags & APR_FOPEN_BINARY));
  return 1;
}

/* file:stat([field, ...]) -> value, ... {{{1
 *
 * This method works like `apr.stat()` except that it uses a file handle
 * instead of a filepath to access the file.
 */

static int file_stat(lua_State *L)
{
  lua_apr_stat_context context = { 0 };
  lua_apr_file *file;
  apr_status_t status;

  file = file_check(L, 1, 1);
  context.firstarg = 2;
  context.lastarg = lua_gettop(L);
  check_stat_request(L, &context);
  status = apr_file_info_get(&context.info, context.wanted, file->handle);
  if (status != APR_SUCCESS && !APR_STATUS_IS_INCOMPLETE(status))
    return push_file_error(L, file, status);

  return push_stat_results(L, &context, NULL);
}

/* file:lines() -> iterator {{{1
 *
 * This function implements the interface of Lua's `file:lines()` function.
 */

static int file_lines(lua_State *L)
{
  lua_apr_file *file = file_check(L, 1, 1);
  return read_lines(L, &file->input);
}

/* file:read([format, ...]) -> mixed value, ... {{{1
 *
 * This function implements the interface of Lua's `file:read()` function.
 */

static int file_read(lua_State *L)
{
  lua_apr_file *file = file_check(L, 1, 1);
  return read_buffer(L, &file->input);
}

/* file:write(value [, ...]) -> status {{{1
 *
 * This function implements the interface of Lua's `file:write()` function.
 */

static int file_write(lua_State *L)
{
  lua_apr_file *file = file_check(L, 1, 1);
  return write_buffer(L, &file->output);
}

/* file:seek([whence [, offset]]) -> offset {{{1
 *
 * This function implements the interface of Lua's `file:seek()` function.
 */

static int file_seek(lua_State *L)
{
  /* TODO Seek the write buffer as well! */

  const char *const modenames[] = { "set", "cur", "end", NULL };
  const apr_seek_where_t modes[] = { APR_SET, APR_CUR, APR_END };

  apr_status_t status;
  lua_apr_file *file;
  lua_apr_buffer *B;
  apr_off_t offset;
  int mode;

  file = file_check(L, 1, 1);
  B = &file->input.buffer;
  mode = modes[luaL_checkoption(L, 2, "cur", modenames)];
  offset = luaL_optlong(L, 3, 0);

  /* XXX Flush write buffer before changing offset! */
  if (!(mode == APR_CUR && offset == 0)) {
    status = flush_buffer(L, &file->output, 1);
    if (status != APR_SUCCESS)
      return push_file_error(L, file, status);
  }

  /* Make relative offsets absolute, adjust for buffered input. */
  if (mode == APR_CUR && B->index < B->limit) {
    apr_off_t temp = 0;
    status = apr_file_seek(file->handle, APR_CUR, &temp);
    if (status != APR_SUCCESS)
      return push_file_error(L, file, status);
    mode = APR_SET, offset = temp - (B->limit - B->index);
  }

  /* Perform the requested seek() operation. */
  status = apr_file_seek(file->handle, mode, &offset);
  if (status != APR_SUCCESS)
    return push_file_error(L, file, status);

  /* Invalidate all buffered input (very inefficient but foolproof: parts of
   * the buffer may have been modified by the binary to text translation).
   * XXX The write buffer has already been reset by flush_buffer() above.
   * FIXME Don't invalidate the buffered input unnecessarily?!
   */
  file->input.buffer.index = 0;
  file->input.buffer.limit = 0;

  /* FIXME Bound to lose precision when APR_FOPEN_LARGEFILE is in effect? */
  lua_pushnumber(L, (lua_Number) offset);

  return 1;
}

/* file:flush() -> status {{{1
 *
 * Saves any written data to @file. On success true is returned, otherwise a
 * nil followed by an error message is returned.
 */

static int file_flush(lua_State *L)
{
  lua_apr_file *file = file_check(L, 1, 1);
  apr_status_t status = flush_buffer(L, &file->output, 0);
  return push_file_status(L, file, status);
}

/* file:lock(type [, nonblocking ]) -> status {{{1
 *
 * Establish a lock on the open file @file. On success true is returned,
 * otherwise a nil followed by an error message is returned. The @type must be
 * one of:
 *
 *  - `'shared'`: Shared lock. More than one process or thread can hold a
 *    shared lock at any given time. Essentially, this is a "read lock",
 *    preventing writers from establishing an exclusive lock
 *  - `'exclusive'`: Exclusive lock. Only one process may hold an exclusive
 *    lock at any given time. This is analogous to a "write lock"
 *
 * If @nonblocking is true the call will not block while acquiring the file
 * lock.
 *
 * The lock may be advisory or mandatory, at the discretion of the platform.
 * The lock applies to the file as a whole, rather than a specific range. Locks
 * are established on a per-thread/process basis; a second lock by the same
 * thread will not block.
 */

static int file_lock(lua_State *L)
{
  const char *options[] = { "shared", "exclusive", NULL };
  const int flags[] = { APR_FLOCK_SHARED, APR_FLOCK_EXCLUSIVE };
  apr_status_t status;
  lua_apr_file *file;
  int type;

  file = file_check(L, 1, 1);
  type = flags[luaL_checkoption(L, 2, NULL, options)];

  if (!lua_isnoneornil(L, 3)) {
    luaL_checktype(L, 3, LUA_TSTRING);
    if (strcmp(lua_tostring(L, 3), "non-blocking") != 0)
      luaL_argerror(L, 3, "invalid option");
    type |= APR_FLOCK_NONBLOCK;
  }
  status = apr_file_lock(file->handle, type);

  return push_file_status(L, file, status);
}

/* file:unlock() -> status {{{1
 *
 * Remove any outstanding locks on the file. On success true is returned,
 * otherwise a nil followed by an error message is returned.
 */

static int file_unlock(lua_State *L)
{
  apr_status_t status;
  lua_apr_file *file;

  file = file_check(L, 1, 1);
  status = apr_file_unlock(file->handle);

  return push_file_status(L, file, status);
}

/* pipe:timeout_get() -> timeout {{{1
 *
 * Get the timeout value or blocking state of @pipe. On success the timeout
 * value is returned, otherwise a nil followed by an error message is returned.
 *
 * The @timeout true means wait forever, false means don't wait at all and a
 * number is the microseconds to wait.
 */

static int pipe_timeout_get(lua_State *L)
{
  lua_apr_file *pipe;
  apr_status_t status;
  apr_interval_time_t timeout;

  pipe = file_check(L, 1, 1);
  status = apr_file_pipe_timeout_get(pipe->handle, &timeout);
  if (status != APR_SUCCESS)
    return push_file_error(L, pipe, status);
  else if (timeout <= 0)
    lua_pushboolean(L, timeout != 0);
  else
    lua_pushinteger(L, (lua_Integer) timeout);
  return 1;
}

/* pipe:timeout_set(timeout) -> status {{{1
 *
 * Set the timeout value or blocking state of @pipe. On success true is
 * returned, otherwise a nil followed by an error message is returned.
 *
 * The @timeout true means wait forever, false means don't wait at all and a
 * number is the microseconds to wait. For example:
 *
 *     -- Configure read end of pipe to block for a maximum of 5 seconds.
 *     pipe:timeout_set(1000000 * 5)
 *     for line in pipe:lines() do
 *       print(line)
 *     end
 */

static int pipe_timeout_set(lua_State *L)
{
  lua_apr_file *pipe;
  apr_status_t status;
  apr_interval_time_t timeout;

  pipe = file_check(L, 1, 1);
  if (lua_isnumber(L, 2))
    timeout = luaL_checkinteger(L, 2);
  else
    timeout = lua_toboolean(L, 2) ? -1 : 0;
  status = apr_file_pipe_timeout_set(pipe->handle, timeout);
  return push_file_status(L, pipe, status);
}

/* file:inherit_set() -> status {{{1
 *
 * Set a file to be inherited by child processes. By default, file descriptors
 * will not be inherited by child processes created by `apr.proc_create()`.
 *
 * At the time of writing this [seems to only apply to UNIX] [inherit] where
 * APR will close all open file handles after performing a [fork()] [fork]
 * unless you explicitly set your files to be inheritable.
 *
 * [inherit]: http://marc.info/?l=apache-httpd-dev&m=104361635329125&w=2
 */

static int file_inherit_set(lua_State *L)
{
  apr_status_t status;
  lua_apr_file *file;

  file = file_check(L, 1, 1);
  status = apr_file_inherit_set(file->handle);

  return push_file_status(L, file, status);
}

/* file:inherit_unset() -> status {{{1
 *
 * Unset a file from being inherited by child processes.
 */

static int file_inherit_unset(lua_State *L)
{
  apr_status_t status;
  lua_apr_file *file;

  file = file_check(L, 1, 1);
  status = apr_file_inherit_unset(file->handle);

  return push_file_status(L, file, status);
}

/* file:close() -> status {{{1
 *
 * Close @file. On success true is returned, otherwise a nil followed by an
 * error message is returned.
 */

static int file_close(lua_State *L)
{
  lua_apr_file *file;
  apr_status_t status;

  file = file_check(L, 1, 1);
  status = file_close_impl(L, file);

  return push_file_status(L, file, status);
}

/* file:__tostring() {{{1 */

static int file_tostring(lua_State *L)
{
  lua_apr_file *file;

  file = file_check(L, 1, 0);
  if (file->handle != NULL)
    lua_pushfstring(L, "%s (%p)", lua_apr_file_type.friendlyname, file->handle);
  else
    lua_pushfstring(L, "%s (closed)", lua_apr_file_type.friendlyname);

  return 1;
}

/* file:__gc() {{{1 */

static int file_gc(lua_State *L)
{
  lua_apr_file *file = file_check(L, 1, 0);
  if (object_collectable((lua_apr_refobj*)file))
    file_close_impl(L, file);
  release_object((lua_apr_refobj*)file);
  return 0;
}

lua_apr_file *file_alloc(lua_State *L, const char *path, lua_apr_pool *refpool) /* {{{1 */
{
  lua_apr_file *file;

  file = new_object(L, &lua_apr_file_type);
  if (refpool == NULL)
    refpool = refpool_alloc(L);
  else
    refpool_incref(refpool);
  file->pool = refpool;
  if (path != NULL)
    path = apr_pstrdup(file->pool->ptr, path);
  file->path = path;

  return file;
}

void init_file_buffers(lua_State *L, lua_apr_file *file, int text_mode) /* {{{1 */
{
  init_buffers(L, &file->input, &file->output, file->handle, text_mode,
      (lua_apr_buf_rf) apr_file_read,
      (lua_apr_buf_wf) apr_file_write,
      (lua_apr_buf_ff) apr_file_flush);
}

lua_apr_file *file_check(lua_State *L, int i, int open) /* {{{1 */
{
  lua_apr_file *file = check_object(L, i, &lua_apr_file_type);
  if (open && file->handle == NULL)
    luaL_error(L, "attempt to use a closed file");
  return file;
}

apr_status_t file_close_impl(lua_State *L, lua_apr_file *file) /* {{{1 */
{
  apr_status_t status = APR_SUCCESS;
  if (file->handle != NULL) {
    status = flush_buffer(L, &file->output, 1);
    if (status == APR_SUCCESS)
      status = apr_file_close(file->handle);
    else
      apr_file_close(file->handle);
    file->handle = NULL;
    refpool_decref(file->pool);
    free_buffer(L, &file->input.buffer);
    free_buffer(L, &file->output.buffer);
  }
  return status;
}

int push_file_status(lua_State *L, lua_apr_file *file, apr_status_t status) /* {{{1 */
{
  if (status == APR_SUCCESS) {
    lua_pushboolean(L, 1);
    return 1;
  } else {
    return push_file_error(L, file, status);
  }
}

int push_file_error(lua_State *L, lua_apr_file *file, apr_status_t status) /* {{{1 */
{
  char message[LUA_APR_MSGSIZE];
  apr_strerror(status, message, count(message));
  lua_pushnil(L);
  if (file->path != NULL)
    lua_pushfstring(L, "%s: %s", file->path, message);
  else
    lua_pushstring(L, message);
  status_to_name(L, status);
  return 3;
}

/* }}}1 */

static luaL_Reg file_methods[] = {
  { "close", file_close },
  { "flush", file_flush },
  { "lock", file_lock },
  { "lines", file_lines },
  { "read", file_read },
  { "seek", file_seek },
  { "stat", file_stat },
  { "unlock", file_unlock },
  { "write", file_write },
  { "timeout_get", pipe_timeout_get },
  { "timeout_set", pipe_timeout_set },
  { "inherit_set", file_inherit_set },
  { "inherit_unset", file_inherit_unset },
  { NULL, NULL }
};

static luaL_Reg file_metamethods[] = {
  { "__tostring", file_tostring },
  { "__eq", objects_equal },
  { "__gc", file_gc },
  { NULL, NULL }
};

lua_apr_objtype lua_apr_file_type = {
  "lua_apr_file*",      /* metatable name in registry */
  "file",               /* friendly object name */
  sizeof(lua_apr_file), /* structure size */
  file_methods,         /* methods table */
  file_metamethods      /* metamethods table */
};

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
