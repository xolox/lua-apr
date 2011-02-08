/* String routines module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: January 3, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 */

#include "lua_apr.h"
#include <apr_strings.h>

/* apr.strnatcmp(left, right) -> status {{{1
 *
 * Do a [natural order comparison] [natsort] of two strings. Returns true when
 * the @left string is less than the @right string, false otherwise. This
 * function can be used as a callback for Lua's standard library function
 * `table.sort()`.
 *
 *     > -- the canonical example:
 *     > list = { 'rfc1.txt', 'rfc2086.txt', 'rfc822.txt' }
 *     > -- collate order:
 *     > table.sort(list)
 *     > for _, name in ipairs(list) do print(name) end
 *     rfc1.txt
 *     rfc2086.txt
 *     rfc822.txt
 *     > -- natural order:
 *     > table.sort(list, apr.strnatcmp)
 *     > for _, name in ipairs(list) do print(name) end
 *     rfc1.txt
 *     rfc822.txt
 *     rfc2086.txt
 *
 * [natsort]: http://sourcefrog.net/projects/natsort/
 */

int lua_apr_strnatcmp(lua_State *L)
{
  const char *a, *b;
  int difference;

  a = luaL_checkstring(L, 1);
  b = luaL_checkstring(L, 2);
  difference = apr_strnatcmp(a, b);
  lua_pushboolean(L, difference < 0);

  return 1;
}

/* apr.strnatcasecmp(left, right) -> status {{{1
 *
 * Like `apr.strnatcmp()`, but ignores the case of the strings.
 */

int lua_apr_strnatcasecmp(lua_State *L)
{
  const char *a, *b;
  int difference;

  a = luaL_checkstring(L, 1);
  b = luaL_checkstring(L, 2);
  difference = apr_strnatcasecmp(a, b);
  lua_pushboolean(L, difference < 0);

  return 1;
}

/* apr.strfsize(number [, padding]) -> readable {{{1
 *
 * Format a binary size positive @number to a compacted human readable string.
 * If the optional @padding argument evaluates to true the resulting string
 * will be padded with spaces to make it four characters wide, otherwise no
 * padding will be applied.
 *
 *     > = apr.strfsize(1024)
 *     '1.0K'
 *     > = apr.strfsize(1024 ^ 2)
 *     '1.0M'
 *     > = apr.strfsize(1024 ^ 3)
 *     '1.0G'
 *
 * Here's a simplified implementation of the [UNIX] [unix] command `ls -l
 * --human-readable` which makes use of the @padding argument to nicely line up
 *  the fields following the size:
 *
 *     function ls(dirpath)
 *       local directory = assert(apr.dir_open(dirpath))
 *       for info in directory:entries() do
 *         io.write(info.protection, ' ')
 *         io.write(info.user, ' ')
 *         io.write(info.group, ' ')
 *         io.write(apr.strfsize(info.size, true), ' ')
 *         io.write(apr.time_format('%Y-%m-%d %H:%I', info.ctime), ' ')
 *         io.write(info.name, '\n')
 *       end
 *       assert(directory:close())
 *     end
 *
 * This is what the result looks like for the [source code directory] [srcdir]
 * of the Lua/APR project:
 *
 *     > ls 'lua-apr/src'
 *     rw-r--r-- peter peter 5.4K 2011-01-02 22:10 apr.lua
 *     rw-r--r-- peter peter 4.7K 2011-01-02 06:06 base64.c
 *     rw-r--r-- peter peter  11K 2010-10-27 13:01 buffer.c
 *     rw-r--r-- peter peter  13K 2011-01-02 21:09 crypt.c
 *     rw-r--r-- peter peter 2.8K 2010-12-31 01:01 date.c
 *     rw-r--r-- peter peter 9.4K 2011-01-01 16:04 dbm.c
 *     rw-r--r-- peter peter 2.5K 2010-09-25 23:11 env.c
 *     rw-r--r-- peter peter  17K 2011-01-02 22:10 errno.c
 *     rw-r--r-- peter peter  10K 2011-01-02 22:10 filepath.c
 *     rw-r--r-- peter peter 1.9K 2011-01-02 04:04 fnmatch.c
 *     rw-r--r-- peter peter  12K 2010-12-31 01:01 io_dir.c
 *     rw-r--r-- peter peter  25K 2011-01-02 04:04 io_file.c
 *     rw-r--r-- peter peter  17K 2010-12-31 01:01 io_net.c
 *     rw-r--r-- peter peter 4.6K 2011-01-02 22:10 io_pipe.c
 *     rw-r--r-- peter peter  11K 2011-01-02 11:11 lua_apr.c
 *     rw-r--r-- peter peter 9.0K 2011-01-02 11:11 lua_apr.h
 *     rw-r--r-- peter peter 6.9K 2010-12-29 14:02 permissions.c
 *     rw-r--r-- peter peter  26K 2011-01-02 22:10 proc.c
 *     rw-r--r-- peter peter 866  2010-10-23 00:12 refpool.c
 *     rw-r--r-- peter peter 4.8K 2010-12-29 14:02 stat.c
 *     rw-r--r-- peter peter 3.5K 2011-01-02 22:10 str.c
 *     rw-r--r-- peter peter 9.8K 2010-12-31 01:01 time.c
 *     rw-r--r-- peter peter 4.7K 2010-09-25 23:11 uri.c
 *     rw-r--r-- peter peter 2.5K 2010-09-25 23:11 user.c
 *     rw-r--r-- peter peter 2.9K 2010-10-22 19:07 uuid.c
 *     rw-r--r-- peter peter 3.8K 2011-01-02 04:04 xlate.c
 *
 * Note: It seems that `apr.strfsize()` doesn't support terabyte range sizes.
 *
 * [srcdir]: https://github.com/xolox/lua-apr/tree/master/src
 */

int lua_apr_strfsize(lua_State *L)
{
  apr_off_t number;
  char buffer[5];
  int padding, offset = 0, length = 4;

  number = (apr_off_t) luaL_checklong(L, 1);
  padding = lua_gettop(L) > 1 && lua_toboolean(L, 2);
  luaL_argcheck(L, number >= 0, 1, "must be non-negative");
  apr_strfsize(number, buffer);
  while (!padding && buffer[offset] == ' ') offset++;
  while (!padding && buffer[length-1] == ' ') length--;
  lua_pushlstring(L, &buffer[offset], length - offset);

  return 1;
}

/* apr.tokenize_to_argv(cmdline) -> arguments {{{1
 *
 * Convert the string @cmdline to a table of arguments. On success the table of
 * arguments is returned, otherwise a nil followed by an error message is
 * returned.
 */

int lua_apr_tokenize_to_argv(lua_State *L)
{
  apr_status_t status;
  apr_pool_t *pool;
  const char *str;
  char **argv;
  int i;

  pool = to_pool(L);
  str  = luaL_checkstring(L, 1);
  status = apr_tokenize_to_argv(str, &argv, pool);

  if (APR_SUCCESS != status)
    return push_error_status(L, status);

  lua_newtable(L);
  for (i = 0; NULL != argv[i]; i++) {
    lua_pushstring(L, argv[i]);
    lua_rawseti(L, -2, i + 1);
  }

  return 1;
}

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
