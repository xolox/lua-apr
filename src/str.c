/* String routines module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: October 22, 2010
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

/* apr.strfsize(number) -> readable {{{1
 *
 * Format a binary size number to a four character compacted human readable
 * string.
 */

int lua_apr_strfsize(lua_State *L)
{
  apr_off_t number;
  char buffer[5];

  number = (apr_off_t) luaL_checklong(L, 1);
  luaL_argcheck(L, number >= 0, 1, "must be non-negative");
  apr_strfsize(number, buffer);
  lua_pushlstring(L, buffer, count(buffer) - 1);

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
