/* Character encoding translation module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: February 13, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 */

#include "lua_apr.h"
#include <apr_xlate.h>

/* Internal functions {{{1 */

static const char *check_codepage(lua_State *L, int idx)
{
  const char *codepage = luaL_checkstring(L, idx);
  return strcmp(codepage, "locale") == 0 ? APR_LOCALE_CHARSET : codepage;
}

/* apr.xlate(input, from, to) -> translated {{{1
 *
 * Translate a string of text from one [character encoding] [charenc] to
 * another. The @from and @to arguments are strings identifying the source and
 * target character encoding. The special value `'locale'` indicates the
 * character set of the [current locale] [locale]. On success the translated
 * string is returned, otherwise a nil followed by an error message is
 * returned.
 *
 * Which character encodings are supported by `apr.xlate()` is system dependent
 * because APR can use both the system's [iconv] [iconv] implementation and the
 * bundled library [apr-iconv] [apr_iconv]. To get a list of valid character
 * encoding names you can look through the [apr-iconv/ccs] [iconv_ccs] and
 * [apr-iconv/ces] [iconv_ces] directories (those are links to the web
 * interface of the apr-iconv repository).
 *
 * [charenc]: http://en.wikipedia.org/wiki/Character_encoding
 * [locale]: http://en.wikipedia.org/wiki/Locale
 * [iconv]: http://en.wikipedia.org/wiki/Iconv
 * [apr_iconv]: http://apr.apache.org/docs/apr-iconv/trunk/group__apr__iconv.html
 * [iconv_ccs]: http://svn.apache.org/viewvc/apr/apr-iconv/trunk/ccs/
 * [iconv_ces]: http://svn.apache.org/viewvc/apr/apr-iconv/trunk/ces/
 */

int lua_apr_xlate(lua_State *L)
{
  apr_pool_t *pool;
  const char *input, *frompage, *topage;
  size_t length, bufsize, extra;
  apr_xlate_t *convset;
  apr_status_t status;
  apr_size_t todo, unused;
  char *output, *temp;

  pool = to_pool(L);
  input = luaL_checklstring(L, 1, &length);
  frompage = check_codepage(L, 2);
  topage = check_codepage(L, 3);

  /* Apparently apr-iconv doesn't like empty input strings. */
  if (length == 0) {
    lua_pushliteral(L, "");
    return 1;
  }

  /* Initialize the output buffer and related variables. */
  output = malloc(bufsize = length);
  if (output == NULL) {
    status = APR_ENOMEM;
    goto fail;
  }
  todo = unused = length;

  /* Initialize the translation context. */
  status = apr_xlate_open(&convset, topage, frompage, pool);
  if (status != APR_SUCCESS)
    goto fail;

  /* Perform the conversion in one or more passes. */
  for (;;) {
    status = apr_xlate_conv_buffer(convset,
        &input[length - todo], &todo,
        &output[bufsize - unused], &unused);
    if (status != APR_SUCCESS)
      goto fail;
    else if (todo == 0)
      break;
    /* Grow the output buffer by one third. */
    extra = bufsize < 10 ? 10 : bufsize / 3;
    temp = realloc(output, bufsize + extra);
    if (temp == NULL) {
      status = APR_ENOMEM;
      goto fail;
    }
    output = temp;
    bufsize += extra;
    unused += extra;
  }

  /* Correctly terminate the output buffer for some multibyte character set encodings. */
  status = apr_xlate_conv_buffer(convset, NULL, NULL,
      &output[bufsize - unused], &unused);
  if (status != APR_SUCCESS)
    goto fail;

  /* Close the translation context. */
  status = apr_xlate_close(convset);
  if (status != APR_SUCCESS)
    goto fail;

  lua_pushlstring(L, output, bufsize - unused);
  free(output);
  /* I'm not sure whether any resources remain in memory after the call to
   * apr_xlate_close(). Just to make sure, we clear the global memory pool
   * now instead of waiting until the next time it's used. */
  apr_pool_clear(pool);
  return 1;

fail:
  free(output);
  apr_pool_clear(pool);
  return push_status(L, status);
}
