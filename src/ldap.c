/* LDAP connection handling module for the Lua/APR binding.
 *
 * Authors:
 *  - zhiguo zhao <zhaozg@gmail.com>
 *  - Peter Odding <peter@peterodding.com>
 *  - Parts of this module were based on LuaLDAP 1.1.0 by Roberto
 *    Ierusalimschy, André Carregal and Tomás Guisasola (license below)
 * Last Change: October 29, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * The Lightweight Directory Access Protocol ([LDAP] [ldap]) enables querying
 * and modifying data hosted on [directory servers] [dirs]. LDAP databases are
 * similar to [relational databases] [reldbs] in the sense that both types of
 * databases store records with attributes and allow clients to search records
 * based on those attributes. Notable differences between LDAP and relational
 * databases are that LDAP stores all records in a [hierarchy] [hierarchy] and
 * records can have an arbitrary number of attributes. LDAP is frequently used
 * by (large) organizations to provide a centralized address book for all
 * employees and to store system account information like user names and
 * passwords in a central place (one piece of the puzzle towards [roaming
 * profiles] [roaming]).
 *
 * [ldap]: http://en.wikipedia.org/wiki/LDAP
 * [dirs]: http://en.wikipedia.org/wiki/Directory_(databases)
 * [reldbs]: http://en.wikipedia.org/wiki/Relational_database
 * [hierarchy]: http://en.wikipedia.org/wiki/Hierarchical_database_model
 * [roaming]: http://en.wikipedia.org/wiki/Roaming_user_profile
 */

#include "lua_apr.h"
#include <apr_ldap.h>
#include <apr_uri.h>

#if APR_HAS_LDAP

/* LuaLDAP license. {{{1
 *
 * The implementation of lua_apr_ldap_search() is based on the LuaLDAP 1.1.0
 * source code whose license is reproduced here in full:
 *
 * LuaLDAP is free software: it can be used for both academic and commercial
 * purposes at absolutely no cost. There are no royalties or GNU-like
 * "copyleft" restrictions. LuaLDAP qualifies as Open Source software. Its
 * licenses are compatible with GPL. LuaLDAP is not in the public domain and
 * the Kepler Project keep its copyright. The legal details are below.
 *
 * The spirit of the license is that you are free to use LuaLDAP for any
 * purpose at no cost without having to ask us. The only requirement is that if
 * you do use LuaLDAP, then you should give us credit by including the
 * appropriate copyright notice somewhere in your product or its documentation.
 *
 * The LuaLDAP library is designed and implemented by Roberto Ierusalimschy,
 * André Carregal and Tomás Guisasola. The implementation is not derived from
 * licensed software.
 *
 * Copyright © 2003-2007 The Kepler Project.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * (This license text was taken from the source code distribution, it's also
 * available online at http://www.keplerproject.org/lualdap/license.html.)
 */

/* Private parts. {{{1
 *
 * Some of the references I've used while working on this module:
 *
 *  - The online OpenLDAP man pages, for example:
 *    http://linux.die.net/man/3/ldap_get_option
 *
 *  - The Python LDAP binding source code
 *    http://genecube.med.yale.edu:8080/downloads/python-ldap-2.3.11/Modules/options.c
 *
 *  - The LuaLDAP source code
 *    http://luaforge.net/plugins/scmcvs/cvsweb.php/lualdap/src/lualdap.c?rev=1.48;cvsroot=lualdap
 */

static apr_pool_t *ldap_pool = NULL;
static int ldap_ssl_inited = 0, ldap_rebind_inited = 0;

/* Types and macros. {{{2 */

/* LDAP objects. */
typedef struct {
  lua_apr_refobj header;
  apr_pool_t *pool;
  LDAP* ldap;
} lua_apr_ldap_object;

/* Union of option value types. */
typedef union {
  int boolean, integer;
  struct timeval *time;
  char *string, **string_array;
} lua_apr_ldap_option;

#define check_ldap_connection(L, idx) \
  (lua_apr_ldap_object*) check_object(L, idx, &lua_apr_ldap_type)

#define raise_ldap_error(L, status) \
  luaL_error(L, ldap_err2string(status))

/* Windows LDAP API compatibility from LuaLDAP. */

#ifndef WINLDAPAPI

  /* All platforms except Windows */
  typedef int ldap_int_t;
  typedef const char * ldap_pchar_t;

#else

  /* Windows compatibility. */
  typedef ULONG ldap_int_t;
  typedef PCHAR ldap_pchar_t;

# define timeval l_timeval
# include <Winber.h>

  /* LDAP_SCOPE_DEFAULT is an OpenLDAP extension, so on Windows we will default
   * to LDAP_SCOPE_SUBTREE instead. */
# ifndef LDAP_SCOPE_DEFAULT
#   define LDAP_SCOPE_DEFAULT LDAP_SCOPE_SUBTREE
# endif

  /* MSDN doesn't mention this function at all. Unfortunately, LDAPMessage an opaque type. */
# define ldap_msgtype(m) ((m)->lm_msgtype)

# define ldap_first_message ldap_first_entry

  /* The WinLDAP API uses ULONG seconds instead of a struct timeval. */
# undef ldap_search_ext

# ifdef UNICODE
#   define ldap_search_ext(ld, base, scope, f, a, o, sc, cc, t, s, msg) \
      ldap_search_extW(ld, base, scope, f, a, o, sc, cc, (t) ? (t)->tv_sec : 0, s, msg)
# else
#   define ldap_search_ext(ld, base, scope, f, a, o, sc, cc, t, s, msg) \
      ldap_search_extA(ld, base, scope, f, a, o, sc, cc, (t) ? (t)->tv_sec : 0, s, msg)
# endif

#endif /* WINLDAPAPI */

/* check_ldap_option() {{{2
 * Mapping between string options, (APR_)LDAP_OPT_* constants and types.
 * Some of the option constants are missing on Windows, hence the #ifdefs.
 */

static struct {
  const char *name;
  const int value;
  const enum {
    LUA_APR_LDAP_TB, /* boolean          */
    LUA_APR_LDAP_TI, /* integer          */
    LUA_APR_LDAP_TT, /* struct timeval   */
    LUA_APR_LDAP_TS  /* string           */
  } type;
} lua_apr_ldap_options[] = {
# ifdef LDAP_OPT_DEFBASE
  { "defbase",          LDAP_OPT_DEFBASE,          LUA_APR_LDAP_TS },
# endif
  { "deref",            LDAP_OPT_DEREF,            LUA_APR_LDAP_TI },
# ifdef LDAP_OPT_NETWORK_TIMEOUT
  { "network-timeout",  LDAP_OPT_NETWORK_TIMEOUT,  LUA_APR_LDAP_TT },
# endif
  { "protocol-version", LDAP_OPT_PROTOCOL_VERSION, LUA_APR_LDAP_TI },
  { "refhop-limit",     APR_LDAP_OPT_REFHOPLIMIT,  LUA_APR_LDAP_TI },
  { "referrals",        APR_LDAP_OPT_REFERRALS,    LUA_APR_LDAP_TB },
  { "restart",          LDAP_OPT_RESTART,          LUA_APR_LDAP_TB },
  { "size-limit",       LDAP_OPT_SIZELIMIT,        LUA_APR_LDAP_TI },
  { "time-limit",       LDAP_OPT_TIMELIMIT,        LUA_APR_LDAP_TI },
# ifdef LDAP_OPT_TIMEOUT
  { "timeout",          LDAP_OPT_TIMEOUT,          LUA_APR_LDAP_TT },
# endif
# ifdef LDAP_OPT_URI
  { "uri",              LDAP_OPT_URI,              LUA_APR_LDAP_TS },
# endif
};

#define ldap_option_value(idx) lua_apr_ldap_options[idx].value
#define ldap_option_type(idx) lua_apr_ldap_options[idx].type

static int check_ldap_option(lua_State *L, int idx)
{
  const char *name;
  int i;

  name = luaL_checkstring(L, idx);
  for (i = 0; i < count(lua_apr_ldap_options); i++)
    if (strcmp(name, lua_apr_ldap_options[i].name) == 0)
      return i;

  return luaL_error(L, lua_pushfstring(L, "invalid option '%s'", name));
}

/* number_to_time() {{{2 */

static struct timeval *number_to_time(lua_State *L, int idx, struct timeval *tv)
{
  if (lua_isnumber(L, idx)) {
    lua_Number seconds = lua_tonumber(L, idx);
    tv->tv_sec = (long) seconds;
    tv->tv_usec = (long) ((seconds - tv->tv_sec) * APR_USEC_PER_SEC);
    return tv;
  } else return NULL;
}

/* push_ldap_status() {{{2
 * Push true or nil followed by error message based on LDAP status code.
 */

static int push_ldap_status(lua_State *L, int status)
{
  if (status == LDAP_SUCCESS) {
    lua_pushboolean(L, 1);
    return 1;
  } else {
    lua_pushnil(L);
    lua_pushstring(L, ldap_err2string(status));
    return 2;
  }
}

/* push_ldap_error() {{{2
 * Push nil followed by error message based on apr_ldap_err_t structure.
 */

static int push_ldap_error(lua_State *L, apr_status_t status, apr_ldap_err_t *error)
{
  if (error == NULL)
    return push_error_status(L, status);

  lua_pushnil(L);
  if (error->reason != NULL && error->msg != NULL) {
    /* "reason" is from APR and "msg" is from the LDAP SDK. */
    lua_pushfstring(L, "%s (%s)", error->reason, error->msg);
    lua_pushinteger(L, error->rc);
  } else if (error->reason != NULL) {
    /* Some APR functions fill in "reason" but not "msg". */
    lua_pushstring(L, error->reason);
    lua_pushinteger(L, error->rc);
  } else {
    /* Not sure this is needed. */
    status_to_message(L, status);
    status_to_name(L, status);
  }

  return 3;
}

/* Search iterator. {{{2 */

static void set_attributes(lua_State *L, LDAP *ld, LDAPMessage *entry, int tab)
{
  BerElement *ber = NULL;
  BerValue **values;
  char *attr;
  int i, n;

  attr = ldap_first_attribute(ld, entry, &ber);
  while (attr != NULL) {
    values = ldap_get_values_len(ld, entry, attr);
    n = ldap_count_values_len(values);
    if (n == 0) /* no values */
      lua_pushboolean(L, 1);
    else if (n == 1) /* just one value */
      lua_pushlstring(L, values[0]->bv_val, values[0]->bv_len);
    else { /* multiple values */
      lua_newtable(L);
      for (i = 0; i < n; i++) {
        lua_pushlstring(L, values[i]->bv_val, values[i]->bv_len);
        lua_rawseti(L, -2, i+1);
      }
    }
    lua_setfield(L, tab, attr);
    ldap_value_free_len(values);
    ldap_memfree(attr);
    attr = ldap_next_attribute(ld, entry, ber);
  }
  ber_free(ber, 0);
}

static void push_distinguished_name(lua_State *L, LDAP *ld, LDAPMessage *entry)
{
  char *dn = ldap_get_dn(ld, entry);
  lua_pushstring(L, dn);
  ldap_memfree(dn);
}

static int search_iterator(lua_State *L)
{
  LDAPMessage *result, *message, *entry;
  lua_apr_ldap_object *object;
  struct timeval *timeout;
  int status, msgid;

  object = lua_touserdata(L, lua_upvalueindex(1));
  msgid = lua_tointeger(L, lua_upvalueindex(2));
  timeout = lua_touserdata(L, lua_upvalueindex(3));
  status = ldap_result(object->ldap, msgid, LDAP_MSG_ONE, timeout, &result);

  if (status == 0)
    raise_error_status(L, APR_TIMEUP);
  else if (status == -1)
    /* TODO Can we get a more specific error (message) here? ld_errno? */
    raise_error_message(L, "Unspecified error");
  else if (status == LDAP_RES_SEARCH_RESULT) {
    /* end of search results */
    return 0;
  } else {
    message = ldap_first_message(object->ldap, result);
    switch (ldap_msgtype(message)) {
      case LDAP_RES_SEARCH_ENTRY:
        entry = ldap_first_entry(object->ldap, message);
        push_distinguished_name(L, object->ldap, entry);
        lua_newtable(L);
        set_attributes(L, object->ldap, entry, lua_gettop(L));
        ldap_msgfree(result);
        return 2;
      /* No reference to LDAP_RES_SEARCH_REFERENCE on MSDN. Maybe is has a replacement? */
#     ifdef LDAP_RES_SEARCH_REFERENCE
      case LDAP_RES_SEARCH_REFERENCE: {
        LDAPMessage *reference = ldap_first_reference(object->ldap, message);
        push_distinguished_name(L, object->ldap, reference); /* is this supposed to work? */
        ldap_msgfree(result);
        return 1;
      }
#     endif
      case LDAP_RES_SEARCH_RESULT:
        /* end of search results */
        ldap_msgfree(result);
        return 0;
      default:
        ldap_msgfree(result);
        raise_error_message(L, "unhandled message type in search results");
    }
  }

  /* shouldn't be reached. */
  ldap_msgfree(result);
  return 0;
}

/* apr.ldap([url [, secure ]]) -> ldap_conn {{{1
 *
 * Create an LDAP connection. The @url argument is a URL string with the
 * following components:
 *
 *  - One of the URL schemes `ldap://` (the default) or `ldaps://` (for secure
 *    connections)
 *  - The host name or IP-address of the LDAP server (defaults to 127.0.0.1)
 *  - An optional port number (defaults to 389)
 *
 * If @secure is true the connection will use [STARTTLS] [starttls] even if the
 * URL scheme is `ldap://`. On success an LDAP connection object is returned,
 * otherwise a nil followed by an error message is returned.
 *
 * [starttls]: http://en.wikipedia.org/wiki/STARTTLS
 */

int lua_apr_ldap(lua_State *L)
{
  lua_apr_ldap_object *object;
  apr_ldap_err_t *error = NULL;
  apr_pool_t *memory_pool;
  apr_status_t status;
  int portno, secure = APR_LDAP_NONE;
  const char *url, *hostname;
  apr_uri_t info;

  lua_settop(L, 2);
  memory_pool = to_pool(L);
  url = luaL_optstring(L, 1, "ldap://127.0.0.1");
  if (lua_toboolean(L, 2))
    secure = APR_LDAP_STARTTLS;

  /* Get and parse the LDAP URL. */
  status = apr_uri_parse(memory_pool, url, &info);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);

  /* Get the host name and port number of the LDAP server. */
  hostname = (info.hostname != NULL) ? info.hostname : "127.0.0.1";
  portno = (info.port_str != NULL) ? info.port : APR_URI_LDAP_DEFAULT_PORT;

  /* Use a secure connection? */
  if (info.scheme != NULL && strcmp(info.scheme, "ldaps") == 0)
    secure = APR_LDAP_SSL;

  /* Create the userdata object and memory pool. */
  object = new_object(L, &lua_apr_ldap_type);
  status = apr_pool_create(&object->pool, NULL);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);

  /* Automatically call apr_ldap_ssl_init() as needed because this
   * stuff is so low level it doesn't make sense to expose it to Lua. */
  if (secure != APR_LDAP_NONE && !ldap_ssl_inited) {
    if (ldap_pool == NULL) {
      /* Create a private memory pool for SSL and rebind support. */
      status = apr_pool_create(&ldap_pool, NULL);
      if (status != APR_SUCCESS)
        return push_error_status(L, status);
    }
    status = apr_ldap_ssl_init(ldap_pool, NULL, 0, &error);
    if (status != APR_SUCCESS)
      return push_error_status(L, status);
    ldap_ssl_inited = 1;
  }

  /* Open the LDAP connection. */
  status = apr_ldap_init(object->pool, &object->ldap, hostname, portno, secure, &error);
  if (status != APR_SUCCESS)
    return push_ldap_error(L, status, error);

  return 1;
}

/* apr.ldap_info() -> string {{{1
 *
 * This function returns a string describing the LDAP [SDK] [sdk] (library)
 * currently in use. On success a string is returned, otherwise a nil followed
 * by an error message is returned. The resulting string is intended to be
 * displayed to the user, it's not meant to be parsed (although you can of
 * course decide to do this :-). According to [apr_ldap.h] [ldap_docs] the
 * following LDAP SDKs can be used:
 *
 *  - Netscape (I assume this been superseded by the Mozilla SDK below)
 *  - Solaris
 *  - [Novell](http://www.novell.com/developer/ndk/ldap_libraries_for_c.html)
 *  - [Mozilla](https://wiki.mozilla.org/Directory)
 *  - [OpenLDAP](http://www.openldap.org/software/man.cgi?query=ldap)
 *  - [Microsoft] [ms_ldap_sdk]
 *  - [Tivoli](http://en.wikipedia.org/wiki/IBM_Tivoli_Directory_Server)
 *  - [zOS](http://www.lsu.edu/departments/ocs/tsc/ldap/ldappref.html)
 *  - 'Others' (implying there is support for other SDKs?)
 *
 * [sdk]: http://en.wikipedia.org/wiki/Software_development_kit
 * [ms_ldap_sdk]: http://msdn.microsoft.com/en-us/library/aa367008(v=vs.85).aspx
 * [ldap_docs]: http://apr.apache.org/docs/apr/trunk/group___a_p_r___util___l_d_a_p.html
 */

int lua_apr_ldap_info(lua_State *L)
{
  apr_pool_t *memory_pool;
  apr_ldap_err_t *result;
  int status;

  memory_pool = to_pool(L);
  status = apr_ldap_info(memory_pool, &result);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);

  lua_pushstring(L, result->reason);
  return 1;
}

/* apr.ldap_url_parse(string) -> table {{{1
 *
 * Parse an [LDAP URL] [ldap_urls] into a table of URL components. On success a
 * table is returned, otherwise a nil followed by an error message and one of
 * the following strings is returned:
 *
 *  - **MEM**: can't allocate memory space
 *  - **PARAM**: parameter is bad
 *  - **BADSCHEME**: URL doesn't begin with `ldap://`, `ldapi://` or `ldaps://`
 *  - **BADENCLOSURE**: URL is missing trailing `>`
 *  - **BADURL**: URL is bad
 *  - **BADHOST**: host port is bad
 *  - **BADATTRS**: bad (or missing) attributes
 *  - **BADSCOPE**: scope string is invalid (or missing)
 *  - **BADFILTER**: bad or missing filter
 *  - **BADEXTS**: bad or missing extensions
 *
 * LDAP URLs look like this:
 *
 *     ldap[is]://host:port[/[dn[?[attributes][?[scope][?[filter][?exts]]]]]]
 *
 * Where:
 *
 *  - @attributes is a comma separated list
 *  - @scope is one of the three strings **base**, **one** or **sub** (the default is **base**)
 *  - @filter is an string-represented filter as in RFC 2254
 *
 * For example:
 *
 *     > = apr.ldap_url_parse 'ldap://root.openldap.org/dc=openldap,dc=org'
 *     {
 *       scheme = 'ldap',
 *       host = 'root.openldap.org',
 *       port = 389,
 *       scope = 'sub',
 *       dn = 'dc=openldap,dc=org',
 *       crit_exts = 0,
 *     }
 *
 * [ldap_urls]: http://en.wikipedia.org/wiki/LDAP#LDAP_URLs
 */

int lua_apr_ldap_url_parse(lua_State*L)
{
  apr_ldap_url_desc_t *ludpp;
  apr_pool_t *memory_pool;
  apr_ldap_err_t *error = NULL;
  const char *url;
  int status, i;
  char *attr, *ext;

  memory_pool = to_pool(L);
  url = luaL_checkstring(L, 1);
  status = apr_ldap_url_parse_ext(memory_pool, url, &ludpp, &error);
  if (status != APR_LDAP_URL_SUCCESS) {
    push_ldap_error(L, status, error);
    lua_pop(L, 1);
    switch (status) {
      case APR_LDAP_URL_ERR_MEM:          lua_pushliteral(L, "MEM");          return 3;
      case APR_LDAP_URL_ERR_PARAM:        lua_pushliteral(L, "PARAM");        return 3;
      case APR_LDAP_URL_ERR_BADSCHEME:    lua_pushliteral(L, "BADSCHEME");    return 3;
      case APR_LDAP_URL_ERR_BADENCLOSURE: lua_pushliteral(L, "BADENCLOSURE"); return 3;
      case APR_LDAP_URL_ERR_BADURL:       lua_pushliteral(L, "BADURL");       return 3;
      case APR_LDAP_URL_ERR_BADHOST:      lua_pushliteral(L, "BADHOST");      return 3;
      case APR_LDAP_URL_ERR_BADATTRS:     lua_pushliteral(L, "BADATTRS");     return 3;
      case APR_LDAP_URL_ERR_BADSCOPE:     lua_pushliteral(L, "BADSCOPE");     return 3;
      case APR_LDAP_URL_ERR_BADFILTER:    lua_pushliteral(L, "BADFILTER");    return 3;
      case APR_LDAP_URL_ERR_BADEXTS:      lua_pushliteral(L, "BADEXTS");      return 3;
      default:                                                                return 2;
    }
  }

  lua_newtable(L);

  lua_pushstring(L, ludpp->lud_scheme);
  lua_setfield(L, -2, "scheme");

  lua_pushstring(L, ludpp->lud_host);
  lua_setfield(L, -2, "host");

  lua_pushinteger(L, ludpp->lud_port);
  lua_setfield(L, -2, "port");

  if (ludpp->lud_scope == LDAP_SCOPE_BASE)
    lua_pushliteral(L, "base");
  else if (ludpp->lud_scope == LDAP_SCOPE_ONELEVEL)
    lua_pushliteral(L, "one");
  else
    lua_pushliteral(L, "sub");
  lua_setfield(L, -2, "scope");

  lua_pushstring(L, ludpp->lud_filter);
  lua_setfield(L, -2, "filter");

  lua_pushstring(L, ludpp->lud_dn);
  lua_setfield(L, -2, "dn");

  lua_pushinteger(L, ludpp->lud_crit_exts);
  lua_setfield(L, -2, "crit_exts");

  if (ludpp->lud_attrs != NULL) {
    i = 0;
    lua_newtable(L);
    while ((attr = ludpp->lud_attrs[i++]) != NULL) {
      lua_pushinteger(L, i + 1);
      lua_pushstring(L, attr);
      lua_settable(L, -3);
    }
    lua_setfield(L, -2, "attrs");
  }

  if (ludpp->lud_exts != NULL) {
    i = 0;
    lua_newtable(L);
    while ((ext = ludpp->lud_exts[i++]) != NULL) {
      lua_pushinteger(L, i + 1);
      lua_pushstring(L, ext);
      lua_settable(L, -3);
    }
    lua_setfield(L, -2, "exts");
  }

  return 1;
}

/* apr.ldap_url_check(url) -> type {{{1
 *
 * Checks whether the given URL is an LDAP URL. On success one of the strings
 * below is returned, otherwise nil is returned:
 *
 *  - **ldap** for regular LDAP URLs (`ldap://`)
 *  - **ldapi** for socket LDAP URLs (`ldapi://`)
 *  - **ldaps** for SSL LDAP URLs (`ldaps://`)
 */

int lua_apr_ldap_url_check(lua_State *L)
{
  const char *url;
 
  url = luaL_checkstring(L, 1);
  if (apr_ldap_is_ldapi_url(url))
    lua_pushliteral(L, "ldapi");
  else if (apr_ldap_is_ldaps_url(url))
    lua_pushliteral(L, "ldaps");
  else if (apr_ldap_is_ldap_url(url))
    lua_pushliteral(L, "ldap");
  else
    lua_pushnil(L);

  return 1;
}

/* ldap_conn:bind([who [, passwd]]) -> status {{{1
 *
 * Bind to the LDAP directory. If no arguments are given an anonymous bind is
 * attempted, otherwise @who should be a string with the relative distinguished
 * name (RDN) of the user in the form `'cn=admin,dc=example,dc=com'`. On
 * success true is returned, otherwise a nil followed by an error message is
 * returned.
 */

static int lua_apr_ldap_bind(lua_State *L)
{
  lua_apr_ldap_object *object;
  const char *who, *passwd;
  int status, version;

  object = check_ldap_connection(L, 1);
  who = luaL_optstring(L, 2, NULL);
  passwd = luaL_optstring(L, 3, NULL);

  /* Default to LDAP v3 */
  version = LDAP_VERSION3;
  status = ldap_set_option(object->ldap, LDAP_OPT_PROTOCOL_VERSION, &version);
  if (status != LDAP_SUCCESS)
    return push_ldap_status(L, status);
  status = ldap_simple_bind_s(object->ldap, (char*)who, (char*)passwd);

  return push_ldap_status(L, status);
}

/* ldap_conn:unbind() -> status {{{1
 *
 * Unbind from the directory. On success true is returned, otherwise a nil
 * followed by an error message is returned.
 */

static int lua_apr_ldap_unbind(lua_State *L)
{
  lua_apr_ldap_object *object;
  int status;

  object = check_ldap_connection(L, 1);
  status = ldap_unbind(object->ldap);

  return push_ldap_status(L, status);
}

/* ldap_conn:option_get(name) -> value {{{1
 *
 * Get an LDAP option by its @name (one of the strings documented below). On
 * success the option value is returned, otherwise a nil followed by an error
 * message is returned. These are the supported LDAP options:
 *
 *  - **defbase** (string)
 *  - **deref** (integer)
 *  - **network-timeout** (fractional number of seconds)
 *  - **protocol-version** (integer)
 *  - **refhop-limit** (integer)
 *  - **referral-urls** (list of strings)
 *  - **referrals** (boolean)
 *  - **restart** (boolean)
 *  - **size-limit** (integer)
 *  - **time-limit** (integer)
 *  - **timeout** (fractional number of seconds)
 *  - **uri** (string with space separated URIs)
 */

static int lua_apr_ldap_option_get(lua_State *L)
{
  lua_apr_ldap_object *object;
  apr_ldap_err_t *error = NULL;
  int optidx, type, status;
  lua_apr_ldap_option value;

  /* Check the arguments. */
  object = check_ldap_connection(L, 1);
  optidx = check_ldap_option(L, 2);

  /* Get the option value. */
  status = apr_ldap_get_option(object->pool, object->ldap,
      ldap_option_value(optidx), &value, &error);
  if (status != APR_SUCCESS)
    return push_ldap_error(L, status, error);

  /* Convert the LDAP C API value to a Lua value. */
  type = ldap_option_type(optidx);
  if (type == LUA_APR_LDAP_TB) {
    /* Boolean. */
    lua_pushboolean(L, (void*)value.boolean == LDAP_OPT_ON);
  } else if (type == LUA_APR_LDAP_TI) {
    /* Integer. */
    lua_pushinteger(L, value.integer);
  } else if (type == LUA_APR_LDAP_TT) {
    /* Time (fractional number of seconds). */
    if (value.time != NULL) {
      lua_pushnumber(L, (lua_Number)value.time->tv_sec +
          ((lua_Number)value.time->tv_usec / APR_USEC_PER_SEC));
      ldap_memfree((void*)value.time);
    } else lua_pushnil(L);
  } else if (type == LUA_APR_LDAP_TS) {
    /* String. */
    if (value.string != NULL) {
      lua_pushstring(L, value.string);
      ldap_memfree(value.string);
    } else lua_pushnil(L);
  } else assert(0);

  return 1;
}

/* ldap_conn:option_set(name, value) -> status {{{1
 *
 * Set the LDAP option @name (one of the strings documented for
 * `ldap_conn:option_get()`) to @value. On success true is returned, otherwise
 * a nil followed by an error message is returned.
 */

static int lua_apr_ldap_option_set(lua_State *L)
{
  lua_apr_ldap_object *object;
  apr_ldap_err_t *error = NULL;
  struct timeval time;
  apr_status_t status;
  int optidx, type, intval;
  void *value;

  object = check_ldap_connection(L, 1);
  optidx = check_ldap_option(L, 2);

  /* Convert the Lua value to an LDAP C API value. */
  type = ldap_option_type(optidx);
  if (type == LUA_APR_LDAP_TB) {
    /* Boolean. */
    value = lua_toboolean(L, 3) ? LDAP_OPT_ON : LDAP_OPT_OFF;
  } else if (type == LUA_APR_LDAP_TI) {
    /* Integer. */
    intval = luaL_checkint(L, 3);
    value = &intval;
  } else if (type == LUA_APR_LDAP_TT) {
    /* Time (fractional number of seconds). */
    luaL_checktype(L, 3, LUA_TNUMBER);
    value = number_to_time(L, 3, &time);
  } else if (type == LUA_APR_LDAP_TS) {
    /* String. */
    value = (void*)luaL_optstring(L, 3, NULL);
  } else assert(0);

  /* Set the option value. */
  status = apr_ldap_set_option(object->pool, object->ldap,
      ldap_option_value(optidx), value, &error);
  if (status != APR_SUCCESS)
    return push_ldap_error(L, status, error);

  lua_pushboolean(L, 1);
  return 1;
}

/* ldap_conn:rebind_add([who [, password]]) -> status {{{1
 *
 * LDAP servers can return referrals to other servers for requests the server
 * itself will not/can not serve. This function creates a cross reference entry
 * for the specified LDAP connection. The rebind callback function will look up
 * this LDAP connection so it can retrieve the @who and @password fields for
 * use in any binds while referrals are being chased.
 *
 * On success true is returned, otherwise a nil followed by an error message is
 * returned.
 *
 * When the LDAP connection is garbage collected the cross reference entry is
 * automatically removed, alternatively `ldap_conn:rebind_remove()` can be
 * called to explicitly remove the entry.
 */

static int lua_apr_ldap_rebind_add(lua_State*L)
{
  /* For LDAP rebind support APR requires a memory pool from the caller to create
   * a mutex. Inspecting the implementation, it appears that this mutex cannot be
   * reinitialized. This means the memory pool must not be destroyed or the LDAP
   * rebind support would break badly! In other words, now follows a known
   * memory leak caused by an apparently borked API :-)
   */
  lua_apr_ldap_object *object;
  const char *who, *password;
  apr_status_t status;

  object = check_ldap_connection(L, 1);
  who = luaL_optstring(L, 2, NULL);
  password = luaL_optstring(L, 3, NULL);

  /* Automatically call apr_ldap_rebind_init() as needed because this
   * stuff is so low level it doesn't make sense to expose it to Lua. */
  if (!ldap_rebind_inited) {
    if (ldap_pool == NULL) {
      /* Create a private memory pool for SSL and rebind support. */
      status = apr_pool_create(&ldap_pool, NULL);
      if (status != APR_SUCCESS)
        return push_error_status(L, status);
    }
    status = apr_ldap_rebind_init(ldap_pool);
    if (status != APR_SUCCESS)
      return push_error_status(L, status);
    ldap_rebind_inited = 1;
  }

  status = apr_ldap_rebind_add(object->pool, object->ldap, who, password);

  return push_status(L, status);
}

/* ldap_conn:rebind_remove() -> status {{{1
 *
 * Explicitly remove an LDAP cross reference entry (also done automatically
 * when the LDAP connection is garbage collected). On success true is returned,
 * otherwise a nil followed by an error message is returned.
 */

static int lua_apr_ldap_rebind_remove(lua_State*L)
{
  lua_apr_ldap_object *object;
  apr_status_t status = APR_SUCCESS;

  object = check_ldap_connection(L, 1);
  if (ldap_rebind_inited)
    status = apr_ldap_rebind_remove(object->ldap);
  /* TODO The original code by zhiguo zhao had "object->ldap = NULL" here. */

  return push_status(L, status);
}

/* ldap_conn:search(parameters) -> iterator {{{1
 *
 * _The implementation of this method is based on LuaLDAP and the following
 * documentation was based on the [LuaLDAP manual] [lualdap]:_
 *
 * Performs a search operation on the directory. The parameters are described
 * below. The search method will return a search iterator which is a function
 * that requires no arguments. The search iterator is used to get the search
 * result and will return a string representing the distinguished name and a
 * table of attributes as returned by the search request.
 *
 * Supported parameters:
 *
 *  - **attrs**: a string or a list of attribute names to be retrieved (default
 *    is to retrieve all attributes)
 *
 *  - **attrsonly**: a boolean value that must be either false (default) if
 *    both attribute names and values are to be retrieved, or true if only
 *    names are wanted
 *
 *  - **base**: The [distinguished name] [distinguished_name] of the entry at
 *    which to start the search
 *
 *  - **filter**: A string representing the search filter as described in [The
 *    String Representation of LDAP Search Filters] [rfc2254] (RFC 2254)
 *
 *  - **scope**: A string indicating the scope of the search. The valid strings
 *    are: _base_, _one_ and _sub_. The empty string and nil will be treated as
 *    the default scope
 *
 *  - **sizelimit**: The maximum number of entries to return (default is no
 *    limit)
 *
 *  - **timeout**: The timeout in seconds (default is no timeout). The
 *    precision is microseconds
 *
 * [distinguished_name]: http://www.keplerproject.org/lualdap/manual.html#dn
 * [rfc2254]: http://www.ietf.org/rfc/rfc2254.txt
 * [lualdap]: http://www.keplerproject.org/lualdap/manual.html#connection
 */

static int lua_apr_ldap_search(lua_State *L)
{
  lua_apr_ldap_object *object;
  int scope, attrsonly, sizelimit;
  int status, i, n, msgid, time_idx;
  struct timeval *timeout;
  ldap_pchar_t base, filter;
  char **attrs;

  lua_settop(L, 2);
  object = check_ldap_connection(L, 1);
  luaL_checktype(L, 2, LUA_TTABLE);

  /* Get the size limit (if any). */
  lua_getfield(L, 2, "sizelimit");
  sizelimit = lua_isnumber(L, -1) ? lua_tointeger(L, -1) : LDAP_NO_LIMIT;
  lua_pop(L, 1);

  /* Check if we're interested in attribute values. */
  lua_getfield(L, 2, "attrsonly");
  attrsonly = lua_toboolean(L, -1);
  lua_pop(L, 1);

  /* Get "base" string. */
  lua_getfield(L, 2, "base");
  base = (ldap_pchar_t) (lua_isstring(L, -1) ? lua_tostring(L, -1) : NULL);
  lua_pop(L, 1);

  /* Get "filter" string. */
  lua_getfield(L, 2, "filter");
  filter = (ldap_pchar_t) (lua_isstring(L, -1) ? lua_tostring(L, -1) : NULL);
  lua_pop(L, 1);

  /* Get timeout value. */
  timeout = lua_newuserdata(L, sizeof *timeout);
  time_idx = lua_gettop(L);
  lua_getfield(L, 2, "timeout");
  timeout = number_to_time(L, -1, timeout);
  lua_pop(L, 1);

  /* Get scope type from string. */
  lua_getfield(L, 2, "scope");
  scope = LDAP_SCOPE_DEFAULT;
  if (lua_isstring(L, -1)) {
    const char *scopename = lua_tostring(L, -1);
    if (strcmp(scopename, "base") == 0)
      scope = LDAP_SCOPE_BASE;
    else if (strcmp(scopename, "one") == 0)
      scope = LDAP_SCOPE_ONELEVEL;
    else if (strcmp(scopename, "sub") == 0)
      scope = LDAP_SCOPE_SUBTREE;
  }
  lua_pop(L, 1);

  /* Get attributes to search for as NULL terminated array of strings. */
  lua_getfield(L, 2, "attrs");
  n = lua_istable(L, -1) ? lua_objlen(L, -1) : 1;
  attrs = lua_newuserdata(L, sizeof attrs[0] * (n + 1));
  if (!lua_istable(L, -1)) {
    attrs[0] = (char*)lua_tostring(L, -1);
    attrs[1] = NULL;
  } else {
    for (i = 0; i < n; i++) {
      lua_rawgeti(L, -1, i + 1);
      attrs[i] = (char*)lua_tostring(L, -1);
      lua_pop(L, 1); /* pop string */
    }
    attrs[n] = NULL;
  }
  lua_pop(L, 1); /* pop "attrs" */

  /* Start the search. */
  status = ldap_search_ext(object->ldap, base, scope, filter, attrs,
      attrsonly, NULL, NULL, timeout, sizelimit, &msgid);
  if (status != LDAP_SUCCESS)
    raise_ldap_error(L, status);

  /* Prepare the search iterator and its upvalues. */
  lua_pushvalue(L, 1);
  lua_pushnumber(L, msgid);
  if (timeout != NULL)
    lua_pushvalue(L, time_idx);
  else
    lua_pushlightuserdata(L, NULL);
  lua_pushcclosure(L, search_iterator, 3);

  return 1;
}

/* tostring(ldap_conn) -> string {{{1 */

static int ldap_tostring(lua_State *L)
{
  lua_apr_ldap_object *object;

  object = check_ldap_connection(L, 1);
  lua_pushfstring(L, "%s (%p)", lua_apr_ldap_type.friendlyname, object);

  return 1;
}

/* ldap_conn:__gc() {{{1 */

static int ldap_gc(lua_State *L)
{
  lua_apr_ldap_object *object;

  object = check_ldap_connection(L, 1);
  if (object->ldap != NULL) {
    apr_pool_destroy(object->pool);
    object->ldap = NULL;
  }

  return 0;
}

/* }}}1 */

static luaL_reg ldap_metamethods[] = {
  { "__tostring", ldap_tostring },
  { "__gc", ldap_gc },
  { NULL, NULL }
};

static luaL_reg ldap_methods[] = {
  { "bind", lua_apr_ldap_bind },
  { "unbind", lua_apr_ldap_unbind },
  { "option_get", lua_apr_ldap_option_get },
  { "option_set", lua_apr_ldap_option_set },
  { "rebind_add", lua_apr_ldap_rebind_add },
  { "rebind_remove", lua_apr_ldap_rebind_remove },
  { "search", lua_apr_ldap_search },
  { NULL, NULL }
};

lua_apr_objtype lua_apr_ldap_type = {
  "lua_apr_ldap_object*",      /* metatable name in registry */
  "LDAP connection",           /* friendly object name */
  sizeof(lua_apr_ldap_object), /* structure size */
  ldap_methods,                /* methods table */
  ldap_metamethods             /* metamethods table */
};

#endif
