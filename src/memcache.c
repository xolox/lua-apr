/* Memcached client module for the Lua/APR binding.
 *
 * Authors:
 *  - zhiguo zhao <zhaozg@gmail.com>
 *  - Peter Odding <peter@peterodding.com>
 * Last Change: May 15, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * [Memcached] [memcached] is a "distributed memory object caching system".
 * It's designed as an in-memory key-value store for small chunks of arbitrary
 * data (strings, objects) from results of database calls, API calls, or page
 * rendering. The memcached client module makes it possible to read from and
 * write to one or more memcached servers over a network socket in Lua.
 *
 * To-do: Find out if "flags" can be any 32 bits value and how useful it is
 * to Lua.
 *
 * [memcached]: http://memcached.org/
 */

#include "lua_apr.h"
#include <apr_lib.h>
#include <apr_memcache.h>

typedef struct {
  lua_apr_refobj header;
  apr_memcache_t *client;
  apr_pool_t *memory_pool;
} lua_apr_memcache_object;

typedef struct {
  lua_apr_refobj header;
  apr_memcache_server_t *server;
} lua_apr_memcache_server_object;

static const apr_uint32_t mc_default_timeout = 60; /* seconds */

/* Macros. {{{1 */

#define check_mc_client(L, idx) \
  ((lua_apr_memcache_object*)check_object(L, idx, &lua_apr_memcache_type))->client

#define check_mc_server(L, idx) \
  ((lua_apr_memcache_server_object*)check_object(L, idx, &lua_apr_memcache_server_type))->server

/* apr.memcache([max_servers]) -> mc_client {{{1
 *
 * Create a memcached client. The optional argument @max_servers determines the
 * maximum number of memcached servers supported by the client (defaults to
 * 10). On success the client object is returned, otherwise nil followed by an
 * error message is returned.
 */

int lua_apr_memcache(lua_State *L)
{
  apr_status_t status;
  lua_apr_memcache_object *object;
  int max_servers;

  max_servers = luaL_optint(L, 1, 10);
  object = new_object(L, &lua_apr_memcache_type);
  status = apr_pool_create(&object->memory_pool, NULL);
  if (status == APR_SUCCESS) {
    status = apr_memcache_create(object->memory_pool, max_servers, 0, &object->client);
    if (status == APR_SUCCESS)
      return 1;
    apr_pool_destroy(object->memory_pool);
    object->memory_pool = NULL;
  }

  return push_error_status(L, status);
}

/* mc_client:hash(str) -> hash {{{1
 * 
 * Create a [CRC32] [crc] hash used to split keys between servers.
 * The hash is not compatible with old memcached clients.
 *
 * [crc]: http://en.wikipedia.org/wiki/Cyclic_redundancy_check
 */

static int mc_hash(lua_State *L)
{
  apr_memcache_t *client;
  const char *data;
  size_t length;

  client = check_mc_client(L, 1);
  data = luaL_checklstring(L, 2, &length);
  lua_pushinteger(L, apr_memcache_hash(client, data, length));

  return 1;
}

/* mc_client:find_server_hash(hash) -> mc_server {{{1
 *
 * Picks a server based on a hash. Returns the info of the server that controls
 * the specified hash.
 */

static int mc_find_server_hash(lua_State *L)
{
  apr_memcache_t *client;
  apr_memcache_server_t *server;
  apr_uint32_t hash;
  lua_apr_memcache_server_object *object;

  client = check_mc_client(L, 1);
  hash = luaL_checkint(L, 2);
  server = apr_memcache_find_server_hash(client, hash);
  if (server == NULL)
    return 0;
  object = new_object(L, &lua_apr_memcache_server_type);
  object->server = server;

  return 1;
}

/* mc_client:add_server(host, port [, min [, smax [, max [, ttl]]]]) -> mc_server {{{1
 *
 * Create a new server object and add it to the client object. On success the
 * server object is returned, otherwise a nil followed by an error message is
 * returned. The parameters have the following meaning:
 *
 *  - @host: hostname of server
 *  - @port: port of server (usually 11211)
 *  - @min:  minimum number of client sockets to open (defaults to 0)
 *  - @smax: soft maximum number of client connections to open (defaults to 1)
 *  - @max:  hard maximum number of client connections (defaults to 1)
 *  - @ttl:  time to live in microseconds of a client connection (defaults to 60000)
 *
 * Note that @min, @smax and @max are only used when APR was compiled with
 * threads. Also a word of caution: Changing servers after startup may cause
 * keys to go to different servers.
 */

static int mc_add_server(lua_State *L)
{
  apr_memcache_t *client;
  apr_memcache_server_t *server;
  apr_status_t status = APR_SUCCESS;
  lua_apr_memcache_server_object *object;

  client = check_mc_client(L, 1);
  if (lua_isuserdata(L, 2)) {
    server = check_mc_server(L, 2);
  } else {
    const char* host = luaL_checkstring(L, 2);
    int port = luaL_checkint(L, 3);
    int min  = luaL_optint(L, 4, 0);
    int smax = luaL_optint(L, 5, 1);
    int max = luaL_optint(L, 6, 1);
    int ttl = luaL_optint(L, 7, 60000); /* 1 minute, 60 second, */
    status = apr_memcache_server_create(client->p, host, port, min, smax, max, ttl, &server);
  }

  if (status == APR_SUCCESS) {
    status = apr_memcache_add_server(client, server);
    if (status == APR_SUCCESS) {
      object = new_object(L, &lua_apr_memcache_server_type);
      object->server = server;
      return 1;
    }
  }

  return push_error_status(L, status);
}

/* mc_client:find_server(host, port) -> mc_server {{{1
 *
 * Finds a server object based on a (hostname, port) pair. On success the
 * server with matching host name and port is returned, otherwise nothing is
 * returned.
 */

static int mc_find_server(lua_State *L)
{
  apr_memcache_t *client;
  const char* host;
  apr_port_t port;
  apr_memcache_server_t *server;
  lua_apr_memcache_server_object *object;

  client = check_mc_client(L, 1);
  host = luaL_checkstring(L, 2);
  port = (apr_port_t)luaL_checkint(L, 3);
  server = apr_memcache_find_server(client, host, port);
  if (server == NULL)
    return 0;
  object = new_object(L, &lua_apr_memcache_server_type);
  object->server = server;

  return 1;
}

/* mc_client:enable_server(mc_server) -> status {{{1
 *
 * Enable a server for use again after disabling the server with
 * `mc_client:disable_server()`. On success true is returned,
 * otherwise nil followed by an error message is returned.
 */

static int mc_enable_server(lua_State *L)
{
  apr_memcache_t *client;
  apr_memcache_server_t *server;
  apr_status_t status;

  client = check_mc_client(L, 1);
  server = check_mc_server(L, 2);
  status = apr_memcache_enable_server(client, server);

  return push_status(L, status);
}

/* mc_client:disable_server(mc_server) -> status {{{1
 *
 * Disable a server. On success true is returned, otherwise nil followed by an
 * error message is returned.
 */

static int mc_disable_server(lua_State *L)
{
  apr_memcache_t *client;
  apr_memcache_server_t *server;
  apr_status_t status;

  client = check_mc_client(L, 1);
  server = check_mc_server(L, 2);
  status = apr_memcache_disable_server(client, server);

  return push_status(L, status);
}

/* mc_client:get(key [, ...]) -> status, value [, ...] {{{1
 *
 * Get one or more values from the server. On success true is returned followed
 * by the retrieved value(s), otherwise nil followed by an error message is
 * returned. The keys are null terminated strings and the return values are
 * binary safe strings. Keys that don't have an associated value result in nil.
 */

static int mc_get(lua_State *L)
{
  apr_memcache_t *client;
  apr_status_t status;
  const char *key;
  apr_size_t len;
  char *value;
  int i, argc;

  client = check_mc_client(L, 1);
  argc = lua_gettop(L);
  lua_pushboolean(L, 1);

  for (i = 2; i <= argc; i++) {
    key = luaL_checkstring(L, i);
    status = apr_memcache_getp(client, client->p, key, &value, &len, NULL);
    if (status == APR_SUCCESS) {
      lua_pushlstring(L, value, len);
    } else if (APR_STATUS_IS_NOTFOUND(status)) {
      lua_pushnil(L);
    } else {
      lua_settop(L, argc);
      push_error_status(L, status);
    }
  }

  return lua_gettop(L) - argc;
}

/* mc_client:set(key, value [, timeout]) -> status {{{1
 *
 * Sets a value by key on the server. If the key already exists the old value
 * will be overwritten. The @key is a null terminated string, the @value is a
 * binary safe string and @timeout is the time in seconds for the data to live
 * on the server (a number, defaults to 60 seconds). On success true is
 * returned, otherwise nil followed by an error message is returned.
 */

static int mc_set(lua_State *L)
{
  apr_memcache_t *client;
  const char *key, *value;
  apr_size_t len;
  apr_uint32_t timeout;
  apr_status_t status;

  client = check_mc_client(L, 1);
  key = luaL_checkstring(L, 2);
  value = luaL_checklstring(L, 3, &len);
  timeout = luaL_optint(L, 4, mc_default_timeout);
  status = apr_memcache_set(client, key, (char*)value, len, timeout, 0);

  return push_status(L, status);
}

/* mc_client:add(key, value [, timeout]) -> status {{{1
 *
 * Adds a value by key on the server. If the key already exists this call will
 * fail and the old value will be preserved. The arguments and return values
 * are documented under `mc_client:set()`.
 */

static int mc_add(lua_State *L)
{
  apr_memcache_t *client;
  const char* key, *value;
  apr_uint32_t timeout;
  apr_status_t status;
  apr_size_t len;

  client = check_mc_client(L, 1);
  key = luaL_checkstring(L, 2);
  value = luaL_checklstring(L, 3, &len);
  timeout = luaL_optint(L, 4, mc_default_timeout);
  status = apr_memcache_add(client, key, (char*)value, len, timeout, 0);

  return push_status(L, status);
}

/* mc_client:replace(key, value [, timeout]) -> status {{{1
 *
 * Replace a value by key on the server. If the key doesn't exist no value will
 * be set. The arguments and return values are documented under
 * `mc_client:set()`.
 */

static int mc_replace(lua_State *L)
{
  apr_memcache_t *client;
  const char* key, *value;
  apr_uint32_t timeout;
  apr_status_t status;
  apr_size_t len;

  client = check_mc_client(L, 1);
  key = luaL_checkstring(L, 2);
  value = luaL_checklstring(L, 3, &len);
  timeout = luaL_optint(L, 4, mc_default_timeout);
  status = apr_memcache_replace(client, key, (char*)value, len, timeout, 0);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);
  lua_pushlstring(L, value, len);

  return 1;
}

/* mc_client:delete(key [, timeout]) -> status {{{1
 *
 * Delete a key from the server. The @key is a null terminated string and
 * @timeout is the time in seconds for the delete to stop other clients from
 * adding (a number, defaults to 10 seconds). On success true is returned,
 * otherwise nil followed by an error message is returned.
 */

static int mc_delete(lua_State *L)
{
  apr_memcache_t *client;
  const char* key;
  apr_uint32_t timeout;
  apr_status_t status;

  client = check_mc_client(L, 1);
  key = luaL_checkstring(L, 2);
  timeout = luaL_optint(L, 3, 10);
  status = apr_memcache_delete(client, key, timeout);

  return push_status(L, status);
}

/* mc_client:incr(key [, number]) -> value {{{1
 *
 * Increment a value. The @key is a null terminated string and the optional
 * argument @number is the number to increment by (defaults to 1). On success
 * the new value after incrementing is returned, otherwise nil followed by an
 * error message is returned.
 */

static int mc_incr(lua_State *L)
{
  apr_memcache_t *client;
  const char* key;
  apr_uint32_t increment_by, resulting_value;
  apr_status_t status;

  client = check_mc_client(L, 1);
  key = luaL_checkstring(L, 2);
  increment_by = luaL_optint(L, 3, 1);
  status = apr_memcache_incr(client, key, increment_by, &resulting_value);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);
  lua_pushinteger(L, resulting_value);

  return 1;
}

/* mc_client:decr(key [, number]) -> value {{{1
 *
 * Decrement a value. The @key is a null terminated string and the optional
 * argument @number is the number to decrement by (defaults to 1). On success
 * the new value after decrementing is returned, otherwise nil followed by an
 * error message is returned.
 */

static int mc_decr(lua_State *L)
{
  apr_memcache_t *client;
  const char* key;
  apr_uint32_t decrement_by, resulting_value;
  apr_status_t status;

  client = check_mc_client(L, 1);
  key = luaL_checkstring(L, 2);
  decrement_by = luaL_checkint(L, 3);
  status = apr_memcache_decr(client, key, decrement_by, &resulting_value);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);
  lua_pushinteger(L, resulting_value);

  return 1;
}

/* mc_client:version(mc_server) -> version {{{1
 *
 * Query a server's version. On success the version string is returned,
 * otherwise nil followed by an error message is returned.
 */

static int mc_version(lua_State *L)
{
  apr_memcache_server_t *server;
  apr_status_t status;
  char* value;
  size_t length;

  (void)check_mc_client(L, 1);
  server = check_mc_server(L, 2);
  status = apr_memcache_version(server, server->p, &value);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);
  length = strlen(value);
  while (apr_isspace(value[length - 1]))
    length--;
  lua_pushlstring(L, value, length);

  return 1;
}

/* mc_client:stats() -> statistics {{{1
 *
 * Query a server for statistics. On success a table with information is
 * returned, otherwise nil followed by an error message is returned. The
 * following fields are supported:
 *
 *  - @version: version string of the server
 *  - @pid: process ID of the server process
 *  - @uptime: number of seconds this server has been running
 *  - @time: current UNIX time according to the server
 *  - @rusage_user: accumulated user time for this process
 *  - @rusage_system: accumulated system time for this process
 *  - @curr_items: current number of items stored by the server
 *  - @total_items: total number of items stored by this server
 *  - @bytes: current number of bytes used by this server to store items
 *  - @curr_connections: number of open connections
 *  - @total_connections: total number of connections opened since the server started running
 *  - @connection_structures: number of connection structures allocated by the server
 *  - @cmd_get: cumulative number of retrieval requests
 *  - @cmd_set: Cumulative number of storage requests
 *  - @get_hits: number of keys that have been requested and found present
 *  - @get_misses: number of items that have been requested and not found
 *  - @evictions: number of items removed from cache because they passed their expiration time
 *  - @bytes_read: total number of bytes read by this server
 *  - @bytes_written: total number of bytes sent by this server
 *  - @limit_maxbytes: number of bytes this server is allowed to use for storage
 *  - @threads: number of threads the server is running (if built with threading)
 */

static int mc_stats(lua_State *L)
{
  apr_memcache_server_t *server;
  apr_memcache_stats_t *stats;
  apr_status_t status;

  (void)check_mc_client(L, 1);
  server = check_mc_server(L, 2);
  status = apr_memcache_stats(server, server->p, &stats);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);

  lua_newtable(L);
  lua_pushstring(L, stats->version);
  lua_setfield(L, -2, "version");

# define setstat(key, value) \
    (lua_pushnumber(L, (lua_Number) value), lua_setfield(L, -2, key))

  setstat("pid", stats->pid);
  setstat("uptime", stats->uptime);
  setstat("time", stats->time);
  setstat("rusage_user", stats->rusage_user);
  setstat("rusage_system", stats->rusage_system);
  setstat("curr_items", stats->curr_items);
  setstat("total_items", stats->total_items);
  setstat("bytes", stats->bytes);
  setstat("curr_connections", stats->curr_connections);
  setstat("total_connections", stats->total_connections);
  setstat("connection_structures", stats->connection_structures);
  setstat("cmd_get", stats->cmd_get);
  setstat("cmd_set", stats->cmd_set);
  setstat("get_hits", stats->get_hits);
  setstat("get_misses", stats->get_misses);
  setstat("evictions", stats->evictions);
  setstat("bytes_read", stats->bytes_read);
  setstat("bytes_written", stats->bytes_written);
  setstat("limit_maxbytes", stats->limit_maxbytes);
  setstat("threads", stats->threads);

  return 1;
}

/* tostring(mc_client) -> string {{{1 */

static int mc_tostring(lua_State *L)
{
  lua_apr_memcache_object *object;

  object = check_object(L, 1, &lua_apr_memcache_type);
  lua_pushfstring(L, "%s (%p)", lua_apr_memcache_type.friendlyname, object->client);

  return 1;
}

/* tostring(mc_server) -> string {{{1 */

static int ms_tostring(lua_State *L)
{
    lua_apr_memcache_server_object *object;

    object = check_object(L, 1, &lua_apr_memcache_server_type);
    lua_pushfstring(L, "%s (%p)", lua_apr_memcache_server_type.friendlyname, object->server);

    return 1;
}

/* mc_client:__gc() {{{1 */

static int mc_gc(lua_State *L)
{
  lua_apr_memcache_object *object;

  object = check_object(L, 1, &lua_apr_memcache_type);
  if (object->memory_pool != NULL) {
    apr_pool_destroy(object->memory_pool);
    object->memory_pool = NULL;
  }

  return 0;
}

/* Internal object definitions. {{{1 */

static luaL_reg mc_methods[] = {
  { "hash", mc_hash },
  { "find_server_hash", mc_find_server_hash },
  { "add_server", mc_add_server },
  { "find_server", mc_find_server },
  { "enable_server", mc_enable_server },
  { "disable_server", mc_disable_server },
  { "get", mc_get },
  { "set", mc_set },
  { "add", mc_add },
  { "replace", mc_replace },
  { "delete", mc_delete },
  { "incr", mc_incr },
  { "decr", mc_decr },
  { "version", mc_version },
  { "stats", mc_stats },
  { NULL, NULL }
};

static luaL_reg mc_metamethods[] = {
  { "__tostring", mc_tostring },
  { "__gc", mc_gc },
  { NULL, NULL }
};

lua_apr_objtype lua_apr_memcache_type = {
  "lua_apr_memcache_t*",           /* metatable name in registry */
  "memcache client",               /* friendly object name */
  sizeof(lua_apr_memcache_object), /* structure size */
  mc_methods,                      /* methods table */
  mc_metamethods                   /* metamethods table */
};

static luaL_reg ms_methods[] = {
  { NULL, NULL }
};

static luaL_reg ms_metamethods[] = {
  { "__tostring", ms_tostring },
  { NULL, NULL }
};

lua_apr_objtype lua_apr_memcache_server_type = {
  "lua_apr_memcache_server_t*",           /* metatable name in registry */
  "memcache server",                      /* friendly object name */
  sizeof(lua_apr_memcache_server_object), /* structure size */
  ms_methods,                             /* methods table */
  ms_metamethods                          /* metamethods table */
};

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
