/* Time routines module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: December 30, 2010
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * Lua represents dates as numbers though the meaning of these numbers is not
 * specified. The manual does state (in the documentation for `os.time()`) that
 * on [POSIX] [posix], Windows and some other systems these numbers count the
 * number of seconds since some given start time called the epoch. This epoch
 * is 00:00:00 January 1, 1970 [UTC] [utc]. The Apache Portable Runtime
 * [represents] [apr_time_t] dates as the number of microseconds since that
 * same epoch. As a compromise between the two units Lua/APR uses seconds but
 * supports sub-second resolution in the decimal part of floating point numbers
 * (see [this thread] [2007-03] on [lua-l] [lua-l] for discussion about the
 * API).
 *
 * [posix]: http://en.wikipedia.org/wiki/POSIX
 * [apr_time_t]: http://apr.apache.org/docs/apr/trunk/group__apr__time.html#gdb4bde16055748190eae190c55aa02bb
 * [2007-03]: http://lua-users.org/lists/lua-l/2007-03/threads.html#00309
 * [lua-l]: http://lua-users.org/lists/lua-l/
 */

#include "lua_apr.h"
#include <apr_time.h>
#include <apr_date.h>

/* Internal functions {{{1 */

static const struct {
  const char *name;
  int byte_offset;
  int value_offset;
} fields[] = {
  { "usec", offsetof(apr_time_exp_t, tm_usec), 0 },
  { "sec", offsetof(apr_time_exp_t, tm_sec), 0 },
  { "min", offsetof(apr_time_exp_t, tm_min), 0 },
  { "hour", offsetof(apr_time_exp_t, tm_hour), 0 },
  { "day", offsetof(apr_time_exp_t, tm_mday), 0 },
  { "month", offsetof(apr_time_exp_t, tm_mon), 1 },
  { "year", offsetof(apr_time_exp_t, tm_year ), 1900 },
  { "wday", offsetof(apr_time_exp_t, tm_wday ), 1 },
  { "yday", offsetof(apr_time_exp_t, tm_yday ), 1 },
  { "gmtoff", offsetof(apr_time_exp_t, tm_gmtoff), 0 },
};

void time_check_exploded(lua_State *L, int idx, apr_time_exp_t *components, int default_to_now)
{
  int i;
  char *field;
  apr_status_t status;

  if (default_to_now && lua_isnoneornil(L, idx)) {
    status = apr_time_exp_lt(components, apr_time_now());
    if (status != APR_SUCCESS)
      raise_error_status(L, status);
  } else if (lua_isnumber(L, idx)) {
    status = apr_time_exp_lt(components, time_get(L, idx));
    if (status != APR_SUCCESS)
      raise_error_status(L, status);
  } else {
    luaL_checktype(L, idx, LUA_TTABLE);
    for (i = 0; i < count(fields); i++) {
      lua_getfield(L, idx, fields[i].name);
      field = (char*)components + fields[i].byte_offset;
      *(apr_int32_t*)field = lua_tointeger(L, -1) - fields[i].value_offset;
      lua_pop(L, 1);
    }
    lua_getfield(L, idx, "isdst");
    components->tm_isdst = lua_toboolean(L, -1);
    lua_pop(L, 1);
  }
}

apr_time_t time_check(lua_State *L, int idx)
{
  if (lua_isnoneornil(L, idx)) {
    return apr_time_now();
  } else if (!lua_istable(L, idx)) {
    luaL_checktype(L, idx, LUA_TNUMBER);
    return time_get(L, idx);
  } else {
    apr_time_t time;
    apr_status_t status;
    apr_time_exp_t components;
    time_check_exploded(L, idx, &components, 1);
    status = apr_time_exp_get(&time, &components);
    if (status != APR_SUCCESS)
      raise_error_status(L, status);
    return time;
  }
}

int time_push(lua_State *L, apr_time_t time)
{
  time_put(L, time);
  return 1;
}

/* apr.sleep(seconds) -> nothing {{{1
 *
 * Sleep for the specified number of @seconds. Sub-second resolution is
 * supported so you can for example give 0.5 to sleep for half a second. This
 * function may sleep for longer than the specified time because of platform
 * limitations.
 */

int lua_apr_sleep(lua_State *L)
{
  luaL_checktype(L, 1, LUA_TNUMBER);
  apr_sleep((apr_interval_time_t) time_get(L, 1));
  return 0;
}

/* apr.time_now() -> time {{{1
 *
 * Get the current time as the number of seconds since 00:00:00 January 1, 1970
 * [UTC] [utc]. If Lua is compiled with floating point support then more
 * precision will be available in the decimal part of the returned number.
 */

int lua_apr_time_now(lua_State *L)
{
  return time_push(L, apr_time_now());
}

/* apr.time_explode([time [, timezone]]) -> components {{{1
 *
 * Convert the numeric value @time (current time if none given) to its human
 * readable components. If @timezone isn't given or evaluates to false the
 * local timezone is used. If its a number then this number is used as the
 * offset in seconds from [GMT] [gmt]. The value true is treated the same as 0,
 * i.e. GMT. On success the table of components is returned, otherwise a nil
 * followed by an error message is returned. The resulting table contains the
 * following fields:
 *
 *  - `usec` is the number of microseconds past `sec`
 *  - `sec` is the number of seconds past `min` (0-61)
 *  - `min` is the number of minutes past `hour` (0-59)
 *  - `hour` is the number of hours past midnight (0-23)
 *  - `day` is the day of the month (1-31)
 *  - `month` is the month of the year (0-11).
 *  - `year` is the year since 1900
 *  - `wday` is the number of days since Sunday (0-6)
 *  - `yday` is the number of days since January 1 (0-365)
 *  - `gmtoff` is the number of seconds east of [UTC] [utc]
 *  - `isdst` is true when [daylight saving time] [dst] is in effect
 *
 * All of these fields are numbers except for `isdst` which is a boolean.
 * Here's an example of the output returned by `apr.time_explode()`:
 *
 *     > -- Note that numeric dates are always in UTC while tables with
 *     > -- date components are in the local timezone by default.
 *     > components = apr.time_explode(1032030336.18671)
 *     > = components
 *     {
 *      usec = 186710,
 *      sec = 36,
 *      min = 5,
 *      hour = 21,
 *      day = 14,
 *      month = 9,
 *      year = 2002,
 *      wday = 7,
 *      yday = 257,
 *      gmtoff = 7200, -- my local timezone
 *      isdst = true,
 *     }
 *     > -- To convert a table of date components back into a number
 *     > -- you can use the apr.time_implode() function as follows:
 *     > = apr.time_implode(components)
 *     1032030336.18671
 *
 * [gmt]: http://en.wikipedia.org/wiki/Greenwich_Mean_Time
 * [utc]: http://en.wikipedia.org/wiki/Coordinated_Universal_Time
 * [dst]: http://en.wikipedia.org/wiki/Daylight_saving_time
 */

int lua_apr_time_explode(lua_State *L)
{
  apr_time_exp_t components;
  apr_status_t status;
  apr_time_t time;
  char *field;
  int i;

  time = time_check(L, 1);
  if (!lua_toboolean(L, 2))
    /* Explode the time according to the local timezone by default. */
    status = apr_time_exp_lt(&components, time);
  else
    /* Or explode the time according to (an offset from) GMT instead. */
    status = apr_time_exp_tz(&components, time,
        lua_isboolean(L, 2) ? 0 : (apr_int32_t) luaL_checkinteger(L, 2));
  if (status != APR_SUCCESS)
    return push_error_status(L, status);

  /* Copy numeric fields of exploded time to Lua table. */
  lua_createtable(L, 0, count(fields) + 1);
  for (i = 0; i < count(fields); i++) {
    field = (char*)&components + fields[i].byte_offset;
    lua_pushinteger(L, *(apr_int32_t*)field + fields[i].value_offset);
    lua_setfield(L, -2, fields[i].name);
  }

  /* Copy boolean `isdst' field. */
  lua_pushboolean(L, components.tm_isdst);
  lua_setfield(L, -2, "isdst");

  return 1;
}

/* apr.time_implode(components) -> time {{{1
 *
 * Convert a table of time @components to its numeric value. On success the
 * time is returned, otherwise a nil followed by an error message is returned.
 * See `apr.time_explode()` for a list of supported components.
 */

int lua_apr_time_implode(lua_State *L)
{
  apr_status_t status;
  apr_time_exp_t components = { 0 };
  apr_time_t time;

  time_check_exploded(L, 1, &components, 0);
  status = apr_time_exp_gmt_get(&time, &components);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);

  return time_push(L, time);
}

/* apr.time_format(format [, time]) -> formatted {{{1
 *
 * Format @time (current time if none given) according to string @format. On
 * success the formatted time is returned, otherwise a nil followed by an error
 * message is returned. The two special formats `'ctime'` and `'rfc822'` result
 * in a fixed length string of 24 or 29 characters in length. The @time
 * argument may be either a number or a table with components like those
 * returned by `apr.time_explode()`.
 *
 *     > = apr.time_format('%Y-%m-%d %H:%I:%S', apr.time_now())
 *     '2010-09-25 17:05:08'
 *     > = apr.time_format('ctime', apr.time_now())
 *     'Sat Sep 25 17:26:22 2010'
 *     > = apr.time_format('rfc822', apr.time_now())
 *     'Sat, 25 Sep 2010 15:26:36 GMT'
 */

int lua_apr_time_format(lua_State *L)
{
  char formatted[1024];
  apr_status_t status;
  const char *format;

  luaL_checktype(L, 1, LUA_TSTRING);
  format = lua_tostring(L, 1);

  if (strcmp(format, "ctime") == 0) {
    status = apr_ctime(formatted, time_check(L, 2));
    if (status != APR_SUCCESS)
      return push_error_status(L, status);
    lua_pushlstring(L, formatted, APR_CTIME_LEN - 1);
    return 1;
  } else if (strcmp(format, "rfc822") == 0) {
    status = apr_rfc822_date(formatted, time_check(L, 2));
    if (status != APR_SUCCESS)
      return push_error_status(L, status);
    lua_pushlstring(L, formatted, APR_RFC822_DATE_LEN - 1);
    return 1;
  } else {
    apr_time_exp_t components = { 0 };
    apr_size_t length = count(formatted);
    time_check_exploded(L, 2, &components, 1);
    status = apr_strftime(formatted, &length, length, format, &components);
    if (status != APR_SUCCESS)
      return push_error_status(L, status);
    lua_pushlstring(L, formatted, length);
    return 1;
  }
}

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
