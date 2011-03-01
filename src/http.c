/* HTTP request parsing module for the Lua/APR binding
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: February 27, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * This module is an experimental binding to the [apreq2] [apreq2] library
 * which enables [HTTP] [http] request parsing of [query strings] [qstrings],
 * [headers] [headers] and [multipart] [multipart] messages. Some general notes
 * about the functions in this module:
 *
 *  - None of the extracted strings (except maybe for request bodies) are
 *    binary safe because (AFAIK) HTTP headers are not binary safe
 *
 *  - Parsed name/value pairs are converted to a Lua table using two rules:
 *    names without a value get the value true and duplicate names result in a
 *    table that collects all values for the given name
 *
 *  - When a parse error is encountered after successfully parsing part of the
 *    input, the results of the function are followed by an error message and
 *    error code (see below)
 *
 * The functions in this module return three values on error: a nil followed by
 * an error message and an error code. This module defines the following error
 * codes (in addition to the [generic Lua/APR error codes](#error_handling)):
 *
 *  - `'EBADARG'`: bad arguments
 *  - `'GENERAL'`: internal apreq2 error
 *  - `'TAINTED'`: attempted to perform unsafe action with tainted data
 *  - `'INTERRUPT'`: parsing interrupted
 *  - `'BADDATA'`: invalid input data
 *  - `'BADCHAR'`: invalid character
 *  - `'BADSEQ'`: invalid byte sequence
 *  - `'BADATTR'`: invalid attribute
 *  - `'BADHEADER'`: invalid header
 *  - `'BADUTF8'`: invalid UTF-8 encoding
 *  - `'NODATA'`: missing input data
 *  - `'NOTOKEN'`: missing required token
 *  - `'NOATTR'`: missing attribute
 *  - `'NOHEADER'`: missing header
 *  - `'NOPARSER'`: missing parser
 *  - `'MISMATCH'`: conflicting information
 *  - `'OVERLIMIT'`: exceeds configured maximum limit
 *  - `'UNDERLIMIT'`: below configured minimum limit
 *  - `'NOTEMPTY'`: setting already configured
 *
 * [apreq2]: http://httpd.apache.org/apreq/
 * [http]: http://en.wikipedia.org/wiki/Hypertext_Transfer_Protocol
 * [qstrings]: http://en.wikipedia.org/wiki/Query_string
 * [headers]: http://en.wikipedia.org/wiki/List_of_HTTP_header_fields
 * [multipart]: http://en.wikipedia.org/wiki/MIME#Multipart_messages
 */

/* TODO Bind the following functions:
 *  - apreq_parse_urlencoded()
 *  - apreq_quote()
 *  - apreq_quote_once()
 *  - apreq_charset_divine()
 */

#include "lua_apr.h"
#include <apreq_error.h>
#include <apreq_cookie.h>
#include <apreq_param.h>
#include <apreq_parser.h>
#include <apreq_util.h>

#define DEFAULT_TABLE_SIZE 10
#define MULTIPART_BRIGADE_LIMIT (1024 ^ 2)

/* Internal functions. {{{1 */

typedef struct {
  lua_State *state;
  apr_pool_t *pool;
  apr_status_t status;
} multipart_context;

/* Return (partial) results (followed by error information). */
#define push_http_result(L, status, nres) \
  (status == APR_SUCCESS ? nres : (nres + push_http_error(L, status, 0)))

/* apreq_init() {{{2 */

static void apreq_init(lua_State *L, apr_pool_t *pool)
{
  /* XXX No static variables here: The APREQ binding will be initialized once
   * for each Lua state that uses it, and this should be fine according to the
   * documentation for apreq_initialize(). Actually I'm not even sure we
   * actually need to initialize anything because we only use explicitly
   * initialized parsers. */

  const char *key = "Lua/APR libapreq";
  apr_status_t status;
  int initialized;

  lua_getfield(L, LUA_ENVIRONINDEX, key);
  initialized = lua_toboolean(L, -1);
  lua_pop(L, 1);
  if (!initialized) {
    status = apreq_initialize(pool);
    if (status != APR_SUCCESS) {
      raise_error_status(L, status);
    } else {
      lua_pushboolean(L, 1);
      lua_setfield(L, LUA_ENVIRONINDEX, key);
    }
  }
}

/* push_http_error() {{{2 */

static int push_http_error(lua_State *L, apr_status_t status, int pushnil)
{
  char buffer[LUA_APR_MSGSIZE];
  apreq_strerror(status, buffer, count(buffer));
  if (pushnil)
    lua_pushnil(L);
  lua_pushstring(L, buffer);
  switch (status) {
    case APR_EBADARG:            lua_pushstring(L, "EBADARG"); break;
    case APREQ_ERROR_GENERAL:    lua_pushstring(L, "GENERAL"); break;
    case APREQ_ERROR_TAINTED:    lua_pushstring(L, "TAINTED"); break;
    case APREQ_ERROR_INTERRUPT:  lua_pushstring(L, "INTERRUPT"); break;
    case APREQ_ERROR_BADDATA:    lua_pushstring(L, "BADDATA"); break;
    case APREQ_ERROR_BADCHAR:    lua_pushstring(L, "BADCHAR"); break;
    case APREQ_ERROR_BADSEQ:     lua_pushstring(L, "BADSEQ"); break;
    case APREQ_ERROR_BADATTR:    lua_pushstring(L, "BADATTR"); break;
    case APREQ_ERROR_BADHEADER:  lua_pushstring(L, "BADHEADER"); break;
    case APREQ_ERROR_BADUTF8:    lua_pushstring(L, "BADUTF8"); break;
    case APREQ_ERROR_NODATA:     lua_pushstring(L, "NODATA"); break;
    case APREQ_ERROR_NOTOKEN:    lua_pushstring(L, "NOTOKEN"); break;
    case APREQ_ERROR_NOATTR:     lua_pushstring(L, "NOATTR"); break;
    case APREQ_ERROR_NOHEADER:   lua_pushstring(L, "NOHEADER"); break;
    case APREQ_ERROR_NOPARSER:   lua_pushstring(L, "NOPARSER"); break;
    case APREQ_ERROR_MISMATCH:   lua_pushstring(L, "MISMATCH"); break;
    case APREQ_ERROR_OVERLIMIT:  lua_pushstring(L, "OVERLIMIT"); break;
    case APREQ_ERROR_UNDERLIMIT: lua_pushstring(L, "UNDERLIMIT"); break;
    case APREQ_ERROR_NOTEMPTY:   lua_pushstring(L, "NOTEMPTY"); break;
    default:                     status_to_name(L, status); break;
  }
  return pushnil ? 3 : 2;
}

/* push_scalars() {{{2 */

static int push_scalars(void *state, const char *key, const char *value)
{
  lua_State *L = state;
  /* Check whether we've seen the key before. */
  lua_getfield(L, -1, key);
  if (lua_isnil(L, -1)) {
    /* 1st occurrence. */
    push_string_or_true(L, value);
    lua_setfield(L, -3, key);
  } else if (lua_isstring(L, -1)) {
    /* 2nd occurrence. */
    lua_newtable(L); /* push subtable */
    lua_pushvalue(L, -1); /* push reference */
    lua_setfield(L, -4, key); /* pop reference */
    lua_insert(L, -2); /* move subtable before 1st value */
    lua_rawseti(L, -2, 1); /* pop 1st value */
    push_string_or_true(L, value); /* push 2nd value */
    lua_rawseti(L, -2, 2); /* pop 2nd value */
  } else {
    /* Later values. */
    push_string_or_true(L, value);
    lua_rawseti(L, -2, lua_objlen(L, -2) + 1);
  }
  lua_pop(L, 1); /* pop nil or subtable */
  return 1;
}

/* push_multipart_headers() {{{2 */

static int push_multipart_headers(void *state, const char *key, const char *value)
{
  lua_State *L = state;
  push_string_or_true(L, value);
  lua_setfield(L, -2, key);
  return 1;
}

/* push_multipart_entry() {{{2 */

static int push_multipart_entry(multipart_context *context, const char *value)
{
  apr_status_t status;
  apr_size_t len;
  lua_State *L = context->state;
  apreq_param_t *param = apreq_value_to_param(value);
  char *upload;

  lua_newtable(L);
  lua_pushstring(L, value);
  lua_rawseti(L, -2, 1);
  apr_table_do(push_multipart_headers, L, param->info, NULL);
  if (param->upload != NULL) {
    status = apr_brigade_pflatten(param->upload, &upload, &len, context->pool);
    if (status != APR_SUCCESS) {
      context->status = status;
      return 0;
    }
    lua_pushlstring(L, upload, len);
    lua_rawseti(L, -2, 2);
  }
  return 1;
}

/* push_multipart_entries() {{{2 */

static int push_multipart_entries(void *ctx, const char *key, const char *value)
{
  multipart_context *context = ctx;
  lua_State *L = context->state;

  /* Check whether we've seen the key before. */
  lua_getfield(L, -1, key);
  if (lua_isnil(L, -1)) {
    /* 1st occurrence. */
    if (!push_multipart_entry(context, value))
      return 0;
    lua_setfield(L, -3, key);
  } else if (lua_isstring(L, -1)) {
    /* 2nd occurrence. */
    lua_newtable(L); /* push subtable */
    lua_pushvalue(L, -1); /* push reference */
    lua_setfield(L, -4, key); /* pop reference */
    lua_insert(L, -2); /* move subtable before 1st value */
    lua_rawseti(L, -2, 1); /* pop 1st value */
    if (!push_multipart_entry(context, value)) /* push 2nd value */
      return 0;
    lua_rawseti(L, -2, 2); /* pop 2nd value */
  } else {
    /* Later values. */
    if (!push_multipart_entry(context, value))
      return 0;
    lua_rawseti(L, -2, lua_objlen(L, -2) + 1);
  }
  lua_pop(L, 1); /* pop nil or subtable */
  return 1;
}

/* apr.parse_headers(request) -> headers, body {{{1
 *
 * Parse the [headers] [headers] in a [HTTP] [http] request string according to
 * [RFC 822] [rfc822]. On success a table of header name/value pairs and the
 * request body string are returned, otherwise nil followed by an error message
 * is returned.
 *
 * There are some gotchas in using this function:
 *
 *  - It will fail if anything comes before the headers, so be sure to strip
 *    the status line from the request string before calling this function
 *
 *  - If the request string doesn't contain an empty line to separate the
 *    headers from the body, the last header might be silently discarded
 *
 * [rfc822]: http://tools.ietf.org/html/rfc822
 */

int lua_apr_parse_headers(lua_State *L)
{
  apr_bucket_alloc_t *allocator;
  apr_bucket_brigade *brigade;
  apr_pool_t *pool;
  apr_size_t bodysize;
  apr_status_t status, parser_status;
  apr_table_t *table;
  apreq_parser_t *parser;
  char *body;
  const char *request;
  size_t requestsize;

  pool = to_pool(L);
  apreq_init(L, pool);
  request = luaL_checklstring(L, 1, &requestsize);

  /* Create the parser and bucket brigade. */
  allocator = apr_bucket_alloc_create(pool);
  parser = apreq_parser_make(pool, allocator, NULL, apreq_parse_headers, 0, NULL, NULL, NULL);
  brigade = apr_brigade_create(pool, allocator);
  
  /* Fill the bucket brigade with the request string. */
  APR_BRIGADE_INSERT_HEAD(brigade, apr_bucket_immortal_create(request, requestsize, allocator));
  APR_BRIGADE_INSERT_TAIL(brigade, apr_bucket_eos_create(allocator));

  /* Run the parser. */
  table = apr_table_make(pool, DEFAULT_TABLE_SIZE);
  parser_status = apreq_parser_run(parser, table, brigade);
  if (parser_status != APR_SUCCESS && apr_is_empty_table(table))
    return push_http_error(L, parser_status, 1);

  /* Create the table with headers. */
  lua_newtable(L);
  apr_table_do(push_scalars, L, table, NULL);

  /* Push the request body. */
  status = apr_brigade_pflatten(brigade, &body, &bodysize, pool);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);
  lua_pushlstring(L, body, bodysize);

  return push_http_result(L, parser_status, 2);
}

/* apr.parse_multipart(request, enctype [, limit [, tempdir]]) -> parts {{{1
 *
 * Parse a [multipart/form-data] [mp_formdata] or [multipart/related]
 * [mp_related] HTTP request body according to [RFC 2388] [rfc2388] and the
 * boundary string in @enctype. On success the table with parameter name/value
 * pairs is returned, otherwise a nil followed by an error message is
 * returned.
 *
 * The optional number @limit gives the maximum in-memory bytes that the data
 * structure used to parse the request may use (it defaults to 1024 KB), but be
 * aware that because of internal copying `apr.parse_multipart()` can use more
 * than double this amount of memory while parsing a request.
 *
 * The optional string @tempdir is the directory used for temporary storage of
 * large uploads.
 *
 * [mp_formdata]: http://en.wikipedia.org/wiki/MIME#Form_Data
 * [mp_related]: http://en.wikipedia.org/wiki/MIME#Related
 * [rfc2388]: http://tools.ietf.org/html/rfc2388
 */

int lua_apr_parse_multipart(lua_State *L)
{
  apr_bucket_alloc_t *allocator;
  apr_bucket_brigade *brigade;
  apr_pool_t *pool;
  apr_size_t brigade_limit;
  apr_status_t status;
  apr_table_t *table;
  apreq_parser_t *parser;
  const char *request, *enctype, *tempdir;
  multipart_context context;
  size_t requestsize;

  pool = to_pool(L);
  apreq_init(L, pool);
  request = luaL_checklstring(L, 1, &requestsize);
  enctype = luaL_checkstring(L, 2);
  tempdir = luaL_optstring(L, 3, NULL);
  brigade_limit = luaL_optint(L, 4, MULTIPART_BRIGADE_LIMIT);

  /* Create the parser and bucket brigade. */
  allocator = apr_bucket_alloc_create(pool);
  parser = apreq_parser_make(pool, allocator, enctype, apreq_parse_multipart, brigade_limit, tempdir, NULL, NULL);
  brigade = apr_brigade_create(pool, allocator);
  
  /* Fill the bucket brigade with the request string. */
  APR_BRIGADE_INSERT_HEAD(brigade, apr_bucket_immortal_create(request, requestsize, allocator));
  APR_BRIGADE_INSERT_TAIL(brigade, apr_bucket_eos_create(allocator));

  /* Run the parser. */
  table = apr_table_make(pool, DEFAULT_TABLE_SIZE);
  status = apreq_parser_run(parser, table, brigade);
  if (status != APR_SUCCESS && apr_is_empty_table(table))
    return push_http_error(L, status, 1);

  /* Create the table with parameters. */
  lua_newtable(L);
  context.state = L;
  context.pool = pool;
  if (!apr_table_do(push_multipart_entries, &context, table, NULL))
    return push_http_error(L, context.status, 1);

  /* Return the parameters and error status (if any). */
  return push_http_result(L, status, 1);
}

/* apr.parse_cookie_header(header) -> cookies {{{1
 *
 * Parse a cookie header and store the cookies in a Lua table. On success the
 * table with cookie name/value pairs is returned, otherwise nil followed by an
 * error message and error code is returned.
 */

int lua_apr_parse_cookie_header(lua_State *L)
{
  apr_status_t status;
  apr_pool_t *pool;
  apr_table_t *table;
  const char *header;

  pool = to_pool(L);
  header = luaL_checkstring(L, 1);
  table = apr_table_make(pool, DEFAULT_TABLE_SIZE);
  status = apreq_parse_cookie_header(pool, table, header);
  if (status != APR_SUCCESS && apr_is_empty_table(table))
    return push_http_error(L, status, 1);
  lua_newtable(L);
  apr_table_do(push_scalars, L, table, NULL);
  return push_http_result(L, status, 1);
}

/* apr.parse_query_string(query_string) -> parameters {{{1
 *
 * Parse a URL encoded string into a Lua table. On success the table with
 * parameter name/value pairs is returned, otherwise nil followed by an error
 * message and error code is returned.
 * 
 * This function uses `&` and `;` as the set of tokens to delineate words, and
 * will treat a word without `=` as a name/value pair with the value true.
 */

int lua_apr_parse_query_string(lua_State *L)
{
  apr_status_t status;
  apr_pool_t *pool;
  apr_table_t *table;
  const char *qs;

  pool = to_pool(L);
  qs = luaL_checkstring(L, 1);
  table = apr_table_make(pool, DEFAULT_TABLE_SIZE);
  status = apreq_parse_query_string(pool, table, qs);
  if (status != APR_SUCCESS && apr_is_empty_table(table))
    return push_http_error(L, status, 1);
  lua_newtable(L);
  apr_table_do(push_scalars, L, table, NULL);
  return push_http_result(L, status, 1);
}

/* apr.header_attribute(header, name) -> value {{{1
 *
 * Search a header string for the value of a particular named attribute. On
 * success the matched value is returned, otherwise nil followed by an error
 * message is returned.
 */

int lua_apr_header_attribute(lua_State *L)
{
  apr_status_t status;
  const char *header, *name, *value;
  size_t namelen;
  apr_size_t valuelen;

  header = luaL_checkstring(L, 1);
  name = luaL_checklstring(L, 2, &namelen);
  status = apreq_header_attribute(header, name, namelen, &value, &valuelen);
  if (status != APR_SUCCESS)
    return push_http_error(L, status, 1);
  lua_pushlstring(L, value, valuelen);
  return 1;
}

/* apr.uri_encode(string) -> encoded {{{1
 *
 * Encode unsafe bytes in @string using [percent-encoding] [percenc] so that
 * the string can be embedded in a [URI] [uri] query string.
 *
 * [percenc]: http://en.wikipedia.org/wiki/Percent-encoding
 */

int lua_apr_uri_encode(lua_State *L)
{
  apr_pool_t *pool;
  const char *string, *encoded;
  size_t length;

  pool = to_pool(L);
  string = luaL_checklstring(L, 1, &length);
  encoded = apreq_escape(pool, string, length);
  lua_pushstring(L, encoded);

  return 1;
}

/* apr.uri_decode(encoded) -> string {{{1
 *
 * Decode all [percent-encoded] [percenc] bytes in the string @encoded.
 */

int lua_apr_uri_decode(lua_State *L)
{
  apr_status_t status;
  apr_pool_t *pool;
  const char *encoded;
  char *string;
  size_t enclen;
  apr_size_t strlen;

  pool = to_pool(L);
  encoded = luaL_checklstring(L, 1, &enclen);
  string = apr_palloc(pool, enclen + 1);
  if (string == NULL)
    return push_error_memory(L);
  memcpy(string, encoded, enclen);

  status = apreq_decode(string, &strlen, encoded, enclen);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);

  lua_pushlstring(L, string, strlen);
  return 1;
}
