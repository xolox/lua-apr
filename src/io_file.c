/* File I/O handling module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: September 25, 2010
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 */

#include "lua_apr.h"
#include <apr_strings.h>
#include <apr_file_io.h>
#include <apr_file_info.h>
#include <apr_lib.h>
#include <stdlib.h>
#include <stdio.h>

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
  target = luaL_checkstring(L, 1);
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
  target = luaL_checkstring(L, 1);
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
  target = luaL_checkstring(L, 1);
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
 *  - `type` is one of the strings `directory`, `file`, `link`, `pipe`,
 *    `socket`, `block device`, `character device` or `unknown`
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
 *  - `link` *is a special flag that does not return a field*, instead it is
 *    used to signal that symbolic links should not be followed, i.e. the
 *    status of the link itself should be returned
 */

int lua_apr_stat(lua_State *L)
{
  const char *path, *name;
  apr_pool_t *memory_pool;
  lua_apr_stat_context context = { 0 };
  apr_status_t status;

  memory_pool = to_pool(L);
  path = luaL_checkstring(L, 1);
  name = apr_filepath_name_get(path);

  context.firstarg = 2;
  context.lastarg = lua_gettop(L);
  check_stat_request(L, &context, STAT_DEFAULT_TABLE);
  status = apr_stat(&context.info, path, context.wanted, memory_pool);

  if (status && !APR_STATUS_IS_INCOMPLETE(status)) {
    return push_error_status(L, status);
  } else {
    const char *parent = apr_pstrndup(memory_pool, path, name - path);
    return push_stat_results(L, &context, parent);
  }
}

#if 0

/* apr.file_open(path [, mode ]) -> file {{{1
 *
 * Open a file for reading and/or writing. This function immitates Lua's
 * io.open() function, so the documentation for io.open() applies:
 *
 * This function opens a file, in the mode specified in the string @mode. It
 * returns a new file handle, or, in case of errors, nil plus an error
 * message.
 *
 * The @mode string can be any of the following:
 *
 * 'r':   read mode (the default);
 * 'w':   write mode;
 * 'a':   append mode;
 * 'r+':  update mode, all previous data is preserved;
 * 'w+':  update mode, all previous data is erased;
 * 'a+':  append update mode, previous data is preserved, writing is only
 *        allowed at the end of file.
 *
 * The @mode string may also have a 'b' at the end, which is needed in some
 * systems to open the file in binary mode. This string is exactly what is used
 * in the standard C function fopen().
 */

int lua_apr_file_open(lua_State *L)
{
  apr_status_t status;
  lua_apr_file *file;
  apr_int32_t flags;
  const char *path;
  char *mode;

  path = luaL_checkstring(L, 1);
  mode = (char *)luaL_optstring(L, 2, "r");
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

  /* Create and return the file object. */
  file = new_object(L, &lua_apr_file_type);
  init_buffer(L, &file->buffer, &file->handle, apr_file_read, apr_file_write);
  status = apr_pool_create(&file->memory_pool, NULL);
  if (APR_SUCCESS != status)
    raise_error_status(L, status);
  status = apr_file_open(&file->handle, path, flags, APR_FPROT_OS_DEFAULT, file->memory_pool);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);
  file->path = apr_pstrdup(file->memory_pool, path);
  return 1;
}

/* file:stat([field, ...]) -> value, ... {{{1
 *
 * This method works like apr.stat() except that it uses a file handle instead
 * of a filepath to access the file.
 */

int file_stat(lua_State *L)
{
  lua_apr_stat_context context = { 0 };
  lua_apr_file *file;
  apr_status_t status;

  file = file_check(L, 1, 1);
  check_stat_request(L, &context, STAT_DEFAULT_TABLE);
  status = apr_file_info_get(&context.info, context.wanted, file->handle);
  if (status && !APR_STATUS_IS_INCOMPLETE(status))
    return push_error_status(L, status);

  return push_stat_results(L, &context, NULL);
}

/* file:read([format, ...]) -> mixed value, ... {{{1 */

int file_read(lua_State *L)
{
  lua_apr_file *file = file_check(L, 1, 1);
  return read_buffer(L, file->buffer);
}

/* file:write(value [, ...]) -> status {{{1 */

int file_write(lua_State *L)
{
  apr_status_t status = APR_SUCCESS;
  lua_apr_file *file;
  const char *data;
  apr_size_t done;
  size_t todo;
  int i, x;



  file = file_check(L, 1, 1);
  for (i = 2, x = lua_gettop(L); i <= x; i++) {
    data = luaL_checklstring(L, i, &todo);
    status = apr_file_write_full(file->handle, data, (apr_size_t)todo, &done);
    if (APR_SUCCESS != status)
      break;
  }
  return push_status(L, status);
}

/* file:seek([whence [, offset]]) -> offset {{{1
 *
 * This method has the same interface as Lua's file:seek() method. Now follows
 * the documentation for file:seek(), copied verbatim, without permission...
 *
 * Sets and gets the file position, measured from the beginning of the file, to
 * the position given by @offset plus a base specified by the string @whence,
 * as follows:
 *
 * 'set':  base is position 0 (beginning of the file);
 * 'cur':  base is current position;
 * 'end':  base is end of file.
 *
 * In case of success, function seek returns the final file position, measured
 * in bytes from the beginning of the file. If this function fails, it returns
 * nil, plus a string describing the error.
 *
 * The default value for @whence is 'cur', and for offset is 0. Therefore, the
 * call file:seek() returns the current file position, without changing it; the
 * call file:seek('set') sets the position to the beginning of the file (and
 * returns 0); and the call file:seek('end') sets the position to the end of
 * the file, and returns its size.
 */

int file_seek(lua_State *L)
{
  const char *const modenames[] = { "set", "cur", "end", 0 };
  const apr_seek_where_t modes[] = { APR_SET, APR_CUR, APR_END };

  apr_status_t status;
  apr_off_t sob, eob, offset;
  lua_apr_file *file;
  int mode;

  file = file_check(L, 1, 1);
  mode = modes[luaL_checkoption(L, 2, "cur", modenames)];
  offset = luaL_optlong(L, 3, 0);

  /* get offsets for start/end of buffer */
  status = apr_file_seek(file->handle, APR_CUR, (eob = 0, &eob));
  if (status != APR_SUCCESS)
    return push_error_status(L, status);
  sob = eob - file->stream.limit;

  if (mode == APR_CUR) {
    /* save an extra seek */
    mode = APR_SET;
    offset += sob + file->stream.index;
  }

  status = apr_file_seek(file->handle, mode, &offset);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);

  /* update buffer */
  if (offset >= sob && offset < eob) {
    /* FIXME: is the cast bad? */
    file->stream.index = (size_t) (offset - sob);
  } else {
    file->stream.limit = 0;
  }

  lua_pushnumber(L, (lua_Number) offset);
  return 1;
}

/* file:flush() -> status {{{1
 *
 * Saves any written data to @file. On success true is returned, otherwise a
 * nil followed by an error message is returned.
 */

int file_flush(lua_State *L)
{
  apr_status_t status;
  lua_apr_file *file;

  file = file_check(L, 1, 1);
  status = apr_file_flush(file->handle);
  return push_status(L, status);
}

/* file:lock(type [, nonblocking ]) -> status {{{1
 *
 * Establish a lock on the open file @file. On success true is returned,
 * otherwise a nil followed by an error message is returned. The @type must be
 * one of:
 *
 * 'shared':     Shared lock. More than one process or thread can hold a
 *               shared lock at any given time. Essentially, this is a
 *               "read lock", preventing writers from establishing an
 *               exclusive lock.
 * 'exclusive':  Exclusive lock. Only one process may hold an exclusive lock
 *               at any given time. This is analogous to a "write lock".
 *
 * If @nonblocking is true the call will not block while acquiring the file
 * lock.
 *
 * The lock may be advisory or mandatory, at the discretion of the platform.
 * The lock applies to the file as a whole, rather than a specific range. Locks
 * are established on a per-thread/process basis; a second lock by the same
 * thread will not block.
 */

int file_lock(lua_State *L)
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

  return push_status(L, status);
}

/* file:unlock() -> status {{{1
 *
 * Remove any outstanding locks on the file. On success true is returned,
 * otherwise a nil followed by an error message is returned.
 */

int file_unlock(lua_State *L)
{
  apr_status_t status;
  lua_apr_file *file;

  file = file_check(L, 1, 1);
  status = apr_file_unlock(file->handle);

  return push_status(L, status);
}

/* file:close() -> status {{{1
 *
 * Close @file. On success true is returned, otherwise a nil followed by an
 * error message is returned.
 */

int file_close(lua_State *L)
{
  lua_apr_file *file;
  apr_status_t status;

  file = file_check(L, 1, 1);
  status = file_close_real(file);

  return push_status(L, status);
}

/* Helpers, file object metamethods and initializer {{{1 */

apr_status_t file_read_real(void *obj, char *buf, apr_size_t *len) /* {{{1 */
{
  lua_apr_file *file = obj;

  return apr_file_read(file->handle, buf, len);
}

apr_status_t file_write_real(void *obj, char *buf, apr_size_t *len) /* {{{1 */
{
  lua_apr_file *file = obj;

  return apr_file_write(file->handle, buf, len);
}

lua_apr_file *file_check(lua_State *L, int i, int open) /* {{{1 */
{
  lua_apr_file *file = luaL_checkudata(L, i, lua_apr_file_type.typename);
  if (open && !file->handle)
    luaL_error(L, "attempt to use a closed file");
  return file;
}

static apr_status_t file_close_real(lua_apr_file *file) /* {{{1 */
{
  apr_status_t status = APR_SUCCESS;
  if (file->handle) {
    status = apr_file_close(file->handle);
    apr_pool_destroy(file->memory_pool);
    free_buffer(&file->buffer);
    file->handle = NULL;
  }
  return status;
}

int file_tostring(lua_State *L) /* {{{1 */
{
  lua_apr_file *file = file_check(L, 1, 0);
  if (file->handle)
    lua_pushfstring(L, "%s (%p)", lua_apr_file_type.typename, file);
  else
    lua_pushfstring(L, "%s (closed)", lua_apr_file_type.typename);
  return 1;
}

int file_gc(lua_State *L) /* {{{1 */
{
  file_close_real(file_check(L, 1, 0));
  return 0;
}

#endif

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
