/* Filename matching module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: January 2, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 */

#include "lua_apr.h"
#include <apr_fnmatch.h>

/* apr.fnmatch(pattern, input [, ignorecase]) -> status {{{1
 *
 * Try to match a string against a filename pattern. When the string matches
 * the pattern true is returned, otherwise false. The supported pattern items
 * are the following subset of shell wild cards:
 *
 *  - `?` matches one character (any character)
 *  - `*` matches zero or more characters (any character)
 *  - `\x` escapes the special meaning of the character `x`
 *  - `[set]` matches one character within set. A range of characters can be
 *    specified by separating the characters of the range with a
 *    <code>-</code> character
 *  - `[^set]` matches the complement of set, where set is defined as above
 *
 * If the optional argument @ignorecase is true, characters are compared
 * case-insensitively.
 */

int lua_apr_fnmatch(lua_State *L)
{
  apr_status_t status;
  const char *pattern, *input;
  int flags;

  pattern = luaL_checkstring(L, 1);
  input = luaL_checkstring(L, 2);
  flags = lua_toboolean(L, 3) ? APR_FNM_CASE_BLIND : 0;
  status = apr_fnmatch(pattern, input, flags);
  lua_pushboolean(L, APR_SUCCESS == status);

  return 1;
}

/* apr.fnmatch_test(pattern) -> status {{{1
 *
 * Determine if a file path @pattern contains one or more of the wild cards
 * that are supported by `apr.fnmatch()`. On success true is returned,
 * otherwise false.
 */

int lua_apr_fnmatch_test(lua_State *L)
{
  const char *string;
  int pattern = 0;

  string = luaL_checkstring(L, 1);
  pattern = apr_fnmatch_test(string);
  lua_pushboolean(L, pattern);

  return 1;
}

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
