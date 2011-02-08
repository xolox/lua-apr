/* Base64 encoding module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: January 2, 2011
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
 * is returned, otherwise a nil followed by an error message is returned. As an
 * example, here is how to convert an image file into a [data: URL]
 * [data_uris]:
 *
 *     > image = io.open 'lua-logo.png'
 *     > encoded_data = apr.base64_encode(image:read '*a')
 *     > data_url = 'data:image/png;base64,' .. encoded_data
 *     > = '<img src="' .. url .. '" width=16 height=16 alt="Lua logo">'
 *     <img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAAXNSR0IArs4c6QAAAm5JREFUOMt9k01LW0EUhp+Z5CaxatSbljRioaQgmCIKoUo3XXRRF241tUuhIEgWIgRSKzhtNLZbKf0BulL6C1x234WgoS0otIoWP4Ji48ed3Dtd9CoSW18YBs45z8uZjyOokVKqGQgBzZZlHQshHM/zGqrVaiCRSGyOjOwmhXDngZgxjAUvwLm5uXC5XI5Ho9G98fHxQ2AXVNS3/QFQLBZjdXXuu7MzegCE4IO4CiulfoK6LSUTxvAcaPX9t4Vg0fMoSskbYxj14ysBgN7e3oRSahPepoUwn4FnQONFd11d8cZstvexbUderq0dKCk5iUTEL63lqCgWi3eklE4+fxYWghXg7tU7aWmJsLExRlNTGIC+voWD5eWtDqUcY9v2sXRdtyGfzx9JyataGKCtLXoJA7S3x2JSOhNKqf1yuXxPuq57DGAML/iHVld3WVpaA6BU2mNxce2yNhgMnkrLsgIw2wLEC4Wn1wyMgaGhT9j2ezo7P7K/fwIQB2VXq9VT6XleFIRXC05OPrncM5mHDAykGB19dLXEC4VCASml/A35I2CL/+jkRHN6qkkm7YvQFqhDx3GapNZa+59iIRAQZLM9DA93U6k4DA6miMVukU4n0NrDtusIhQIIwfyFt1BKtaVSqZ1MptQoBF+AJDdrwxjSs7NhEQwGHamU2iqVSg9AHRpDP7B+A7xuDP3GTB2dn5/X53K5ivQH6HuhUOgA9dUYuo3hNbAKaH+tGiMm/uamvk1PT99PpVI7AKJmEpPAtlLqzH9EPy8MwMzMTEJrHfh75Ix7zcA3abUsy9VaG8AGDgEHiNbX1+/lcrnK1fo/txYAMvuVJrYAAAAASUVORK5CYII=" width=16 height=16 alt="Lua logo">
 *
 * This is what the result looks like (might not work in older web browsers):
 * <img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAAXNSR0IArs4c6QAAAm5JREFUOMt9k01LW0EUhp+Z5CaxatSbljRioaQgmCIKoUo3XXRRF241tUuhIEgWIgRSKzhtNLZbKf0BulL6C1x234WgoS0otIoWP4Ji48ed3Dtd9CoSW18YBs45z8uZjyOokVKqGQgBzZZlHQshHM/zGqrVaiCRSGyOjOwmhXDngZgxjAUvwLm5uXC5XI5Ho9G98fHxQ2AXVNS3/QFQLBZjdXXuu7MzegCE4IO4CiulfoK6LSUTxvAcaPX9t4Vg0fMoSskbYxj14ysBgN7e3oRSahPepoUwn4FnQONFd11d8cZstvexbUderq0dKCk5iUTEL63lqCgWi3eklE4+fxYWghXg7tU7aWmJsLExRlNTGIC+voWD5eWtDqUcY9v2sXRdtyGfzx9JyataGKCtLXoJA7S3x2JSOhNKqf1yuXxPuq57DGAML/iHVld3WVpaA6BU2mNxce2yNhgMnkrLsgIw2wLEC4Wn1wyMgaGhT9j2ezo7P7K/fwIQB2VXq9VT6XleFIRXC05OPrncM5mHDAykGB19dLXEC4VCASml/A35I2CL/+jkRHN6qkkm7YvQFqhDx3GapNZa+59iIRAQZLM9DA93U6k4DA6miMVukU4n0NrDtusIhQIIwfyFt1BKtaVSqZ1MptQoBF+AJDdrwxjSs7NhEQwGHamU2iqVSg9AHRpDP7B+A7xuDP3GTB2dn5/X53K5ivQH6HuhUOgA9dUYuo3hNbAKaH+tGiMm/uamvk1PT99PpVI7AKJmEpPAtlLqzH9EPy8MwMzMTEJrHfh75Ix7zcA3abUsy9VaG8AGDgEHiNbX1+/lcrnK1fo/txYAMvuVJrYAAAAASUVORK5CYII=" width=16 height=16 alt="Lua logo">
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
