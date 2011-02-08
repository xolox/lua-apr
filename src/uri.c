/* Uniform resource identifier parsing module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: September 25, 2010
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 */

#include "lua_apr.h"
#include <apr_uri.h>
#include <apr_strings.h>

const static struct {
  const char *name;
  const int offset;
} fields[] = {
  { "scheme",   offsetof(apr_uri_t, scheme  ) },
  { "hostinfo", offsetof(apr_uri_t, hostinfo) },
  { "user",     offsetof(apr_uri_t, user    ) },
  { "password", offsetof(apr_uri_t, password) },
  { "hostname", offsetof(apr_uri_t, hostname) },
  { "port",     offsetof(apr_uri_t, port_str) },
  { "path",     offsetof(apr_uri_t, path    ) },
  { "query",    offsetof(apr_uri_t, query   ) },
  { "fragment", offsetof(apr_uri_t, fragment) }
};

/* apr.uri_parse(uri) -> components {{{1
 *
 * Parse the [Uniform Resource Identifier] [uri] @uri. On success a table of
 * components is returned, otherwise a nil followed by an error message is
 * returned. The table of components can have the following fields, all
 * strings:
 *
 *  - `scheme` is the part of the URI before `://` (as in `http`, `ftp`, etc.)
 *  - `user` is the user name, as in `scheme://user:pass@host:port/`
 *  - `password` is the password, as in `scheme://user:pass@host:port/`
 *  - `hostinfo` is the combined `[user[:password]@]hostname[:port]`
 *  - `hostname` is the host name or IP address
 *  - `port` is the port number
 *  - `path` is the request path (`/` if only `scheme://hostname` was given)
 *  - `query` is everything after a `?` in the path, if present
 *  - `fragment` is the trailing `#fragment` string, if present
 *
 * [uri]: http://en.wikipedia.org/wiki/Uniform_Resource_Identifier
 */

int lua_apr_uri_parse(lua_State *L)
{
  apr_status_t status;
  apr_pool_t *memory_pool;
  apr_uri_t components = { NULL };
  const char *string;
  int i;

  memory_pool = to_pool(L);
  string = luaL_checkstring(L, 1);
  status = apr_uri_parse(memory_pool, string, &components);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);

  lua_newtable(L);
  for (i = 0; i < count(fields); i++) {
    char *field = *(char **) ((char *) &components + fields[i].offset);
    if (field && *field) {
      lua_pushstring(L, fields[i].name);
      lua_pushstring(L, field);
      lua_rawset(L, -3);
    }
  }
  return 1;
}

/* apr.uri_unparse(components [, option]) -> uri {{{1
 *
 * Convert a table of [URI] [uri] @components into a URI string. On success the
 * URI string is returned, otherwise a nil followed by an error message is
 * returned. The list of fields in the @components table is available in the
 * documentation for `apr.uri_parse()`. The argument @option may be one of the
 * following:
 *
 *  - `hostinfo` to unparse the components `[user[:password]@]hostname[:port]`
 *  - `pathinfo` to unparse the components `path[?query[#fragment]]`
 */

int lua_apr_uri_unparse(lua_State *L)
{
  const char *options[] = { "hostinfo", "pathinfo", "default" };
  const int values[] = {
    APR_URI_UNP_OMITPATHINFO | APR_URI_UNP_REVEALPASSWORD,
    APR_URI_UNP_OMITSITEPART,
    APR_URI_UNP_REVEALPASSWORD
  };

  apr_uri_t components = { 0 };
  apr_pool_t *memory_pool;
  int i, flags = 0;

  memory_pool = to_pool(L);
  luaL_checktype(L, 1, LUA_TTABLE);
  flags = values[luaL_checkoption(L, 2, "default", options)];

  for (i = 0; i < count(fields); i++) {
    lua_getfield(L, 1, fields[i].name);
    if (lua_isstring(L, -1)) {
      char **field = (char **)((char *) &components + fields[i].offset);
      *field = apr_pstrdup(memory_pool, lua_tostring(L, -1));
    }
    lua_pop(L, 1);
  }

  /* .port_str and .port must both be set, otherwise :port
   * will not be included in the unparsed components! (I think) */
  if (components.port_str)
    components.port = (apr_port_t) atoi(components.port_str);

  lua_pushstring(L, apr_uri_unparse(memory_pool, &components, flags));
  return 1;
}

/* apr.uri_port_of_scheme(scheme) -> port {{{1
 *
 * Return the default port for the given [URI] [uri] @scheme string. [Since at
 * least APR 1.2.8] [uri_ports] the following schemes are supported: `acap`,
 * `ftp`, `gopher`, `http`, `https`, `imap`, `ldap`, `nfs`, `nntp`, `pop`,
 * `prospero`, `rtsp`, `sip`, `snews`, `ssh`, `telnet`, `tip`, `wais`,
 * `z39.50r` and `z39.50s`.
 *
 * [uri_ports]: http://svn.apache.org/viewvc/apr/apr/trunk/uri/apr_uri.c?view=markup#l43
 */

int lua_apr_uri_port_of_scheme(lua_State *L)
{
  const char *scheme;
  int port;

  scheme = luaL_checkstring(L, 1);
  port = apr_uri_port_of_scheme(scheme);
  if (0 == port)
    return 0;
  lua_pushnumber(L, port);
  return 1;
}

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
