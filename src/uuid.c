/* Universally unique identifiers module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: October 22, 2010
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * [Universally unique identifiers] [uuid] are a standard for generating unique
 * strings that are specific to the machine on which they are generated and/or
 * the time at which they are generated. They can be used as primary keys in
 * databases and are used to uniquely identify file system types and instances
 * on modern operating systems. This is what a standard format UUID looks like:
 *
 *     > = apr.uuid_format(apr.uuid_get())
 *     '0ad5d4a4-591e-41f7-8be4-07d7961a8079'
 *
 * [uuid]: http://en.wikipedia.org/wiki/Universally_unique_identifier
 */

#include "lua_apr.h"
#include <apr_uuid.h>

#define APR_UUID_LENGTH sizeof(apr_uuid_t)

/* apr.uuid_get() -> binary {{{1
 *
 * Generate and return a UUID as a binary string of 16 bytes.
 */

int lua_apr_uuid_get(lua_State *L)
{
  apr_uuid_t uuid;
  apr_uuid_get(&uuid);
  lua_pushlstring(L, (const char *) uuid.data, sizeof uuid);
  return 1;
}

/* apr.uuid_format(binary) -> formatted {{{1
 *
 * Format a UUID of 16 bytes following the standard format of 32 hexadecimal
 * digits, displayed in 5 groups separated by hyphens, in the form `8-4-4-4-12`
 * for a total of 36 characters, like `f5dc3464-6c8f-654e-a407-b15b7a30f038`.
 * On success the formatted UUID is returned, otherwise a nil followed by an
 * error message is returned.
 */

int lua_apr_uuid_format(lua_State *L)
{
  size_t length;
  const char *uuid;
  char formatted[APR_UUID_FORMATTED_LENGTH + 1];

  uuid = luaL_checklstring(L, 1, &length);

  if (APR_UUID_LENGTH != length) {
    const char *msg = "expected string of %d characters";
    luaL_argerror(L, 1, lua_pushfstring(L, msg, APR_UUID_LENGTH));
  }

  /* pointer to structure == pointer to first element */
  apr_uuid_format(formatted, (const apr_uuid_t *) uuid);
  lua_pushlstring(L, formatted, APR_UUID_FORMATTED_LENGTH);
  return 1;
}

/* apr.uuid_parse(formatted) -> binary {{{1
 *
 * Parse a standard format UUID and return its 16-byte equivalent. On success
 * the parsed UUID is returned, otherwise a nil followed by an error message is
 * returned.
 */

int lua_apr_uuid_parse(lua_State *L)
{
  apr_status_t status;
  size_t length;
  const char *formatted;
  apr_uuid_t uuid;

  formatted = luaL_checklstring(L, 1, &length);

  if (APR_UUID_FORMATTED_LENGTH != length) {
    const char *msg = "expected string of %d characters";
    luaL_argerror(L, 1, lua_pushfstring(L, msg, APR_UUID_FORMATTED_LENGTH));
  }

  status = apr_uuid_parse(&uuid, formatted);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);
  lua_pushlstring(L, (const char *) uuid.data, APR_UUID_LENGTH);
  return 1;
}

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
