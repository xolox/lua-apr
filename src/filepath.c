/* File path manipulation module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: March 27, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 */

#include "lua_apr.h"
#include <apr_file_info.h>
#include <apr_portable.h>
#include <apr_lib.h>
#include <apr_strings.h>

static apr_int32_t check_options(lua_State *L, int idx) /* {{{1 */
{
  const char *options[] = { "true-name", "native", NULL };
  const apr_int32_t values[] = { APR_FILEPATH_TRUENAME, APR_FILEPATH_NATIVE };
  apr_int32_t flags = 0;

  while (!lua_isnoneornil(L, idx))
    flags |= values[luaL_checkoption(L, idx++, NULL, options)];

  return flags;
}

/* apr.filepath_root(path [, option, ...]) -> root, path {{{1
 *
 * Extract the root from the file path @path. On success the extracted root and
 * the relative path following the root are returned, otherwise a nil followed
 * by an error message is returned. Either or both of the following options may
 * be given after @path:
 *
 *  - `'true-name'` verifies that the root exists and resolves its true case.
 *    If the root does not exist, a nil followed by an error message is
 *    returned
 *  - `'native'` uses the file system's native path format (e.g. path delimiters
 *    of `:` on MacOS9, <code>\\</code> on Win32, etc.) in the resulting root
 *
 * These options only influence the resulting root. The path after the root is
 * returned as is. If you want to convert a whole file path to its true case
 * and/or native format use `apr.filepath_merge()` instead.
 */

int lua_apr_filepath_root(lua_State *L)
{
  apr_pool_t *memory_pool;
  const char *root, *path;
  apr_status_t status;
  apr_int32_t flags;

  memory_pool = to_pool(L);
  path = luaL_checkstring(L, 1);
  flags = check_options(L, 2);

  status = apr_filepath_root(&root, &path, flags, memory_pool);
  if (status != APR_SUCCESS && !APR_STATUS_IS_INCOMPLETE(status))
    return push_error_status(L, status);

  lua_pushstring(L, root);
  lua_pushstring(L, path);
  return 2;
}

/* apr.filepath_parent(path [, option, ...]) -> parent, filename {{{1
 *
 * Split the file path @path into its parent path and filename. This function
 * supports the same options as `apr.filepath_root()`. If any options are given
 * they're applied to @path before it is split, so the options influence both
 * of the resulting values. If @path is a filename and the `'true-name'` option
 * isn't given then the returned parent path will be an empty string.
 */

int lua_apr_filepath_parent(lua_State *L)
{
  apr_pool_t *memory_pool;
  apr_status_t status;
  apr_int32_t flags;
  const char *input, *root, *path, *name;
  size_t length;
  char *buffer;

  memory_pool = to_pool(L);
  input = path = luaL_checkstring(L, 1);
  flags = check_options(L, 2);

  /* Check if the path is rooted because we don't want to damage the root. */
  status = apr_filepath_root(&root, &input, flags, memory_pool);
  if (status == APR_SUCCESS || APR_STATUS_IS_INCOMPLETE(status))
    path = input;
  else
    root = NULL;

  /* Copy the path to a writable buffer. */
  if (flags == 0) {
    buffer = apr_pstrdup(memory_pool, path);
  } else {
    /* In the process we normalize the path as well. */
    status = apr_filepath_merge(&buffer, NULL, path, flags, memory_pool);
    if (status != APR_SUCCESS)
      return push_error_status(L, status);
  }

  /* Ignore empty trailing path segments so we don't return an empty name (2nd
   * return value) unless the 1st return value is a root path. On UNIX we don't
   * want to strip the leading "/" so the first character is ignored. */
  length = strlen(buffer);
  while (length > 0)
      if (buffer[length - 1] == '/')
        length--;
# ifdef WIN32
      else if (buffer[length - 1] == '\\')
        length--;
# endif
      else
        break;

  /* Recombine root and path. */
  buffer[length] = '\0';
  status = apr_filepath_merge(&buffer, root, buffer, flags, memory_pool);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);

  /* Finally we're ready to get the parent path... */
  name = apr_filepath_name_get(buffer);
  lua_pushlstring(L, buffer, name - buffer);
  lua_pushstring(L, name);

  return 2;
}

/* apr.filepath_name(path [, split]) -> filename [, extension] {{{1
 *
 * Extract the filename (the final element) from the file path @path. If @split
 * evaluates true then the extension will be split from the filename and
 * returned separately. Some examples of what is considered a filename or an
 * extension:
 *
 *     > -- regular file path
 *     > = apr.filepath_name('/usr/bin/lua', true)
 *     'lua', ''
 *
 *     > -- hidden file on UNIX
 *     > = apr.filepath_name('/home/xolox/.vimrc', true)
 *     '.vimrc', ''
 *
 *     > -- multiple extensions
 *     > = apr.filepath_name('index.html.en', true)
 *     'index.html', '.en'
 */

int lua_apr_filepath_name(lua_State *L)
{
  const char *path, *name, *ext = NULL;

  path = luaL_checkstring(L, 1);
  name = apr_filepath_name_get(path);

  if (!lua_toboolean(L, 2)) {
    lua_pushstring(L, name);
    return 1;
  } else {
    ext = strrchr(name, '.');
    if (!ext || ext == name)
      ext = name + strlen(name);
    lua_pushlstring(L, name, ext - name);
    lua_pushstring(L, ext);
    return 2;
  }
}

/* apr.filepath_merge(root, path [, option, ...]) -> merged {{{1
 *
 * Merge the file paths @root and @path. On success the merged file path is
 * returned, otherwise a nil followed by an error message is returned. Any
 * combination of one or more of the following options may be given:
 *
 *  - `'true-name'` resolves the true case of existing elements in @path,
 *    resolving any aliases on Windows, and appends a trailing slash if the
 *    final element is a directory
 *  - `'native'` uses the file system's native path format (e.g. path delimiters
 *    of `:` on MacOS9, <code>\\</code> on Win32, etc.)
 *  - `'not-above-root'` fails if @path is above @root, e.g. if @root is
 *    `/foo/bar` and @path is `../baz`
 *  - `'not-absolute'` fails if the merged path is absolute
 *  - `'not-relative'` fails if the merged path is relative
 *  - `'secure-root'` fails if @path is above @root, even given the @root
 *    `/foo/bar` and the @path `../bar/bash`
 *
 * This function can be used to generate absolute file paths as follows:
 *
 *     apr.filepath_merge('.', 'filepath.c', 'not-relative')
 *     -- the above is equivalent to the below:
 *     apr.filepath_merge(apr.filepath_get(), 'filepath.c', 'not-relative')
 *
 */

int lua_apr_filepath_merge(lua_State *L)
{
  const char *options[] = {
    "true-name", "native",
    "not-above-root", "not-absolute",
    "not-relative", "secure-root", NULL };

  const apr_int32_t values[] = {
    APR_FILEPATH_TRUENAME, APR_FILEPATH_NATIVE,
    APR_FILEPATH_NOTABOVEROOT, APR_FILEPATH_NOTABSOLUTE,
    APR_FILEPATH_NOTRELATIVE, APR_FILEPATH_SECUREROOT };

  apr_pool_t *memory_pool;
  const char *root, *path;
  apr_status_t status;
  apr_int32_t flags;
  char *merged;
  int arg;

  memory_pool = to_pool(L);
  root = luaL_checkstring(L, 1);
  path = luaL_checkstring(L, 2);
  if (strcmp(root, ".") == 0)
    root = NULL;

  for (arg = 3, flags = 0; !lua_isnoneornil(L, arg); arg++)
    flags |= values[luaL_checkoption(L, arg, 0, options)];

  status = apr_filepath_merge(&merged, root, path, flags, memory_pool);

  if (status != APR_SUCCESS && !APR_STATUS_IS_EPATHWILD(status))
    return push_error_status(L, status);

  lua_pushstring(L, merged);
  return 1;
}

/* apr.filepath_list_split(searchpath) -> components {{{1
 *
 * Split a search path string into a table of separate components. On success
 * the table of components is returned, otherwise a nil followed by an error
 * message is returned. Empty elements do not become part of the returned
 * table.
 *
 * An example of a search path is the [$PATH] [path_var] environment variable
 * available in UNIX and Windows operating systems which controls the way
 * program names are resolved to absolute pathnames (see
 * `apr.filepath_which()`).
 *
 * [path_var]: http://en.wikipedia.org/wiki/PATH_(variable)
 */

int lua_apr_filepath_list_split(lua_State *L)
{
  apr_array_header_t *array;
  apr_pool_t *memory_pool;
  apr_status_t status;
  const char *liststr;
  int i;

  memory_pool = to_pool(L);
  liststr = luaL_checkstring(L, 1);
  status = apr_filepath_list_split(&array, liststr, memory_pool);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);

  /* The APR array type is documented to be opaque and the only function to get
   * values from it uses stack semantics so we (ab)use Lua's stack to reverse
   * the order of the strings in the list (since order is VERY significant) */

  lua_createtable(L, 0, array->nelts);
  for (i = 0; i < array->nelts; i++) {
    lua_pushstring(L, ((char **)array->elts)[i]);
    lua_rawseti(L, -2, i + 1);
  }
  return 1;
}

/* apr.filepath_list_merge(components) -> searchpath {{{1
 *
 * Merge a table of search path components into a single search path string. On
 * success the table of components is returned, otherwise a nil followed by an
 * error message is returned.
 */

int lua_apr_filepath_list_merge(lua_State *L)
{
  apr_array_header_t *array;
  unsigned int i, count;
  apr_pool_t *memory_pool;
  apr_status_t status;
  char *list;

  memory_pool = to_pool(L);
  luaL_checktype(L, 1, LUA_TTABLE);
  count = (unsigned int) lua_objlen(L, 1);
  array = apr_array_make(memory_pool, count, sizeof(char *));
  if (!array)
    return push_error_memory(L);

  for (i = 1; i <= count; i++) {
    lua_rawgeti(L, -1, i);
    if (!lua_isstring(L, -1)) {
      const char *fmt = "expected string value at index " LUA_QL("%i") ", got %s";
      luaL_argerror(L, 1, lua_pushfstring(L, fmt, i, luaL_typename(L, -1)));
    } else {
      const char **top = apr_array_push(array);
      if (!top)
        return push_error_memory(L);
      *top = apr_pstrdup(memory_pool, lua_tostring(L, -1));
      lua_pop(L, 1);
    }
  }

  status = apr_filepath_list_merge(&list, array, memory_pool);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);
  lua_pushstring(L, list ? list : "");
  return 1;
}

/* apr.filepath_get([native]) -> path {{{1
 *
 * Get the default filepath for relative filenames. If @native evaluates true
 * the file system's native path format is used. On success the filepath is
 * returned, otherwise a nil followed by an error message is returned.
 *
 * On at least Windows and UNIX the default file path for relative file names
 * is the current working directory. Because some operating systems supported
 * by APR don't use this convention Lua/APR uses the term 'default filepath'
 * instead.
 */

int lua_apr_filepath_get(lua_State *L)
{
  apr_pool_t *memory_pool;
  apr_status_t status;
  apr_int32_t flags;
  char *path;

  memory_pool = to_pool(L);
  flags = lua_toboolean(L, 1) ? APR_FILEPATH_NATIVE : 0;
  status = apr_filepath_get(&path, flags, memory_pool);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);

  lua_pushstring(L, path);
  return 1;
}

/* apr.filepath_set(path) -> status {{{1
 *
 * Set the default file path for relative file names to the string @path. On
 * success true is returned, otherwise a nil followed by an error message is
 * returned. Also see the notes for `apr.filepath_get()`.
 */

int lua_apr_filepath_set(lua_State *L)
{
  apr_pool_t *memory_pool;
  apr_status_t status;
  const char *path;

  memory_pool = to_pool(L);
  path = luaL_checkstring(L, 1);
  status = apr_filepath_set(path, memory_pool);

  return push_status(L, status);
}

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
