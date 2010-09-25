/* Base64 encoding module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: September 25, 2010
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * The functions in this module can be used to encode strings in base64 and to
 * decode base64 encoded strings. The base64 format uses the printable
 * characters `A-Z`, `a-z`, `0-9`, `+` and `/` to encode binary data. This can
 * be useful when your data is used in a context that isn't 8-bit clean, for
 * example in e-mail attachments and [data:] [data_uris] URLs. You can read
 * more about base64 encoding in [this] [base64] Wikipedia article.
 *
 * [base64]: http://en.wikipedia.org/wiki/Base64
 * [data_uris]: http://en.wikipedia.org/wiki/Data_URI_scheme
 */

#include "lua_apr.h"
#include <apr_base64.h>

/* apr.base64_encode(plain) -> coded {{{1
 *
 * Encode the string @plain using base64 encoding. On success the coded string
 * is returned, otherwise a nil followed by an error message is returned.
 */

int lua_apr_base64_encode(lua_State *L)
{
  size_t plain_len, coded_len;
  apr_pool_t *memory_pool;
  const char *plain;
  char *coded;

  memory_pool = to_pool(L);
  plain = luaL_checklstring(L, 1, &plain_len);
  coded_len = apr_base64_encode_len(plain_len);
  coded = apr_palloc(memory_pool, coded_len);
  if (coded == NULL)
    return push_error_memory(L);
  coded_len = apr_base64_encode(coded, plain, plain_len);
  if (coded_len > 0 && coded[coded_len - 1] == '\0')
    coded_len--;
  lua_pushlstring(L, coded, coded_len);
  return 1;
}

/* apr.base64_decode(coded) -> plain {{{1
 *
 * Decode the base64 encoded string @coded. On success the decoded string is
 * returned, otherwise a nil followed by an error message is returned.
 */

int lua_apr_base64_decode(lua_State *L)
{
  size_t plain_len, coded_len;
  apr_pool_t *memory_pool;
  const char *coded;
  char *plain;

  memory_pool = to_pool(L);
  coded = luaL_checklstring(L, 1, &coded_len);
  plain_len = apr_base64_decode_len(coded);
  plain = apr_palloc(memory_pool, plain_len);
  if (plain == NULL)
    return push_error_memory(L);
  plain_len = apr_base64_decode(plain, coded);
  if (plain_len > 0 && plain[plain_len - 1] == '\0')
    plain_len--;
  lua_pushlstring(L, plain, plain_len);
  return 1;
}

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
