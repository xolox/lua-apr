/* Date parsing module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: February 13, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 */

#include "lua_apr.h"
#include <apr_date.h>

/* apr.date_parse_http(string) -> time {{{1
 *
 * Parses an [HTTP] [http] date in one of three standard forms:
 *
 *  - `'Sun, 06 Nov 1994 08:49:37 GMT'` - [RFC 822] [rfc822], updated by [RFC 1123][rfc1123]
 *  - `'Sunday, 06-Nov-94 08:49:37 GMT'` - [RFC 850] [rfc850], obsoleted by [RFC 1036][rfc1036]
 *  - `'Sun Nov  6 08:49:37 1994'` - ANSI C's [asctime()] [asctime] format
 *
 * On success the date is returned as a number like documented under [time
 * routines](#time_routines). If the date string is out of range or invalid nil
 * is returned.
 *
 * [http]: http://en.wikipedia.org/wiki/Hypertext_Transfer_Protocol
 * [rfc822]: http://tools.ietf.org/html/rfc822
 * [rfc1123]: http://tools.ietf.org/html/rfc1123
 * [rfc850]: http://tools.ietf.org/html/rfc850
 * [rfc1036]: http://tools.ietf.org/html/rfc1036
 * [asctime]: http://linux.die.net/man/3/asctime
 */

int lua_apr_date_parse_http(lua_State *L)
{
  const char *input;
  apr_time_t result;

  input = luaL_checkstring(L, 1);
  result = apr_date_parse_http(input);
  if (result == 0)
    return 0;
  time_put(L, result);
  return 1;
}

/* apr.date_parse_rfc(string) -> time {{{1
 *
 * Parses a string resembling an [RFC 822] [rfc822] date. This is meant to be
 * lenient in its parsing of dates and hence will parse a wider range of dates
 * than `apr.date_parse_http()`.
 *
 * The prominent mailer (or poster, if mailer is unknown) that has been seen in
 * the wild is included for the unknown formats:
 *
 *  - `'Sun, 06 Nov 1994 08:49:37 GMT'` - [RFC 822] [rfc822], updated by [RFC 1123] [rfc1123]
 *  - `'Sunday, 06-Nov-94 08:49:37 GMT'` - [RFC 850] [rfc850], obsoleted by [RFC 1036] [rfc1036]
 *  - `'Sun Nov  6 08:49:37 1994'` - ANSI C's [asctime()] [asctime] format
 *  - `'Sun, 6 Nov 1994 08:49:37 GMT'` - [RFC 822] [rfc822], updated by [RFC 1123] [rfc1123]
 *  - `'Sun, 06 Nov 94 08:49:37 GMT'` - [RFC 822] [rfc822]
 *  - `'Sun, 6 Nov 94 08:49:37 GMT'` - [RFC 822] [rfc822]
 *  - `'Sun, 06 Nov 94 08:49 GMT'` - Unknown [drtr@ast.cam.ac.uk]
 *  - `'Sun, 6 Nov 94 08:49 GMT'` - Unknown [drtr@ast.cam.ac.uk]
 *  - `'Sun, 06 Nov 94 8:49:37 GMT'` - Unknown [Elm 70.85]
 *  - `'Sun, 6 Nov 94 8:49:37 GMT'` - Unknown [Elm 70.85]
 *
 * On success the date is returned as a number like documented under [time
 * routines](#time_routines). If the date string is out of range or invalid nil
 * is returned.
 */

int lua_apr_date_parse_rfc(lua_State *L)
{
  const char *input;
  apr_time_t result;

  input = luaL_checkstring(L, 1);
  result = apr_date_parse_rfc(input);
  if (result == 0)
    return 0;
  time_put(L, result);
  return 1;
}

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
