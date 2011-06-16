/* DBM routines module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: June 16, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * This module enables the creation and manipulation of [dbm] [dbm] databases.
 * If you've never heard of dbm before you can think of it as a Lua table that
 * only supports string keys and values but is backed by a file, so that you
 * can stop and restart your application and find the exact same contents.
 * This module supports the following libraries/implementations:
 *
 *  - `'db'` for [Berkeley DB] [bdb] files
 *  - `'gdbm'` for GDBM files
 *  - `'ndbm'` for NDBM files
 *  - `'sdbm'` for SDBM files (the default)
 *
 * The SDBM format is the default database format for Lua/APR because APR has
 * built-in support for SDBM while the other libraries need to be separately
 * installed. This is why not all types may be available at run time.
 *
 * [dbm]: http://en.wikipedia.org/wiki/dbm
 * [bdb]: http://en.wikipedia.org/wiki/Berkeley_DB
 */

#include "lua_apr.h"
#include <apr_dbm.h>

/* XXX Default to sdbm because it's always available. */
#define LUA_APR_DBM_DEFAULT "sdbm"

/* Structure for DBM objects. */
typedef struct {
  lua_apr_refobj header;
  apr_pool_t *pool;
  apr_dbm_t *handle;
  const char *path;
} lua_apr_dbm;

/* Internal functions {{{1 */

/* dbm_check(L, i, open) -- get dbm object from Lua stack {{{2 */

static lua_apr_dbm *dbm_check(lua_State *L, int i, int open)
{
  lua_apr_dbm *dbm = check_object(L, i, &lua_apr_dbm_type);
  if (open && dbm->handle == NULL)
    luaL_error(L, "attempt to use a closed dbm handle");
  return dbm;
}

/* dbmtype_check(L, i) -- {{{2 */

static const char *dbmtype_check(lua_State *L, int i)
{
  const char *options[] = { "db", "gdbm", "ndbm", "sdbm", NULL };
  return options[luaL_checkoption(L, i, LUA_APR_DBM_DEFAULT, options)];
}

/* datum_check(L, i, datum) -- get datum object from Lua stack {{{2 */

static void datum_check(lua_State *L, int i, apr_datum_t *d)
{
  size_t size;
  /* XXX I assume key datums are never modified by callees?! */
  d->dptr = (char*)luaL_checklstring(L, i, &size);
  d->dsize = size;
}

/* dbm_close_impl(L, dbm) -- close dbm handle {{{2 */

static void dbm_close_impl(lua_State *L, lua_apr_dbm *dbm)
{
  if (dbm->handle != NULL) {
    apr_dbm_close(dbm->handle);
    dbm->handle = NULL;
  }
  if (dbm->pool != NULL) {
    apr_pool_destroy(dbm->pool);
    dbm->pool = NULL;
  }
}

/* apr.dbm_open(path [, mode [, permissions [, type ]]]) -> dbm object {{{1
 *
 * Open a [dbm] [dbm] file by @path. On success a database object is returned,
 * otherwise a nil followed by an error message is returned. The following
 * @mode strings are supported:
 *
 *  - `'r'` to open an existing database for reading only (this is the default)
 *  - `'w'` to open an existing database for reading and writing
 *  - `'c'` to open a database for reading and writing, creating it if it
 *    doesn't exist
 *  - `'n'` to open a database for reading and writing, truncating it if it
 *    already exists
 *
 * The @permissions string is documented elsewhere. Valid values for @type are
 * listed in the introductory text for this module. Also note that the @path
 * string may not be a real file name, as many [dbm] [dbm] packages append
 * suffixes for separate data and index files (see also
 * `apr.dbm_getnames()`).
 */

int lua_apr_dbm_open(lua_State *L)
{
  const char *options[] = { "r", "w", "c", "n" };
  const apr_int32_t values[] = {
    APR_DBM_READONLY, APR_DBM_READWRITE,
    APR_DBM_RWCREATE, APR_DBM_RWTRUNC
  };
  const char *path, *type;
  apr_int32_t mode;
  apr_fileperms_t perm;
  lua_apr_dbm *object;
  apr_status_t status;

  path = luaL_checkstring(L, 1);
  mode = values[luaL_checkoption(L, 2, "r", options)];
  perm = check_permissions(L, 3, 1);
  type = dbmtype_check(L, 4);
  object = new_object(L, &lua_apr_dbm_type);
  object->path = path;
  status = apr_pool_create(&object->pool, NULL);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);
  status = apr_dbm_open_ex(&object->handle, type, path, mode, perm, object->pool);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);

  return 1;
}

/* apr.dbm_getnames(path [, type]) -> used1 [, used2] {{{1
 *
 * If the specified @path were passed to `apr.dbm_open()`, return the actual
 * pathnames which would be (created and) used. At most, two files may be used;
 * @used2 is nil if only one file is used. The [dbm] [dbm] file(s) don't need
 * to exist because this function only manipulates the pathnames. Valid values
 * for @type are listed in the introductory text for this module.
 */

int lua_apr_dbm_getnames(lua_State *L)
{
  apr_pool_t *pool;
  const char *path, *type, *used1 = NULL, *used2 = NULL;
  apr_status_t status;

  pool = to_pool(L);
  path = luaL_checkstring(L, 1);
  type = dbmtype_check(L, 2);

  /* XXX Use apr_dbm_get_usednames_ex() instead of apr_dbm_get_usednames()
   * because the latter returns void while in reality it can fail and not set
   * the used1 variable! :-\ */
  status = apr_dbm_get_usednames_ex(pool, type, path, &used1, &used2);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);
  lua_pushstring(L, used1);
  if (used2 == NULL)
    return 1;
  lua_pushstring(L, used2);
  return 2;
}

/* dbm:exists(key) -> status {{{1
 *
 * Check whether the [dbm] [dbm] record with the given string @key exists.
 */

int dbm_exists(lua_State *L)
{
  lua_apr_dbm *object;
  apr_datum_t key;

  object = dbm_check(L, 1, 1);
  datum_check(L, 2, &key);
  lua_pushboolean(L, apr_dbm_exists(object->handle, key));

  return 1;
}

/* dbm:fetch(key) -> value {{{1
 *
 * Fetch the [dbm] [dbm] record with the given string @key. On success the
 * fetched value is returned as a string, if the key doesn't exist nothing is
 * returned and on error a nil followed by an error message is returned.
 */

int dbm_fetch(lua_State *L)
{
  lua_apr_dbm *object;
  apr_datum_t key, val;
  apr_status_t status;

  object = dbm_check(L, 1, 1);
  datum_check(L, 2, &key);
  status = apr_dbm_fetch(object->handle, key, &val);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);
  else if (val.dsize == 0)
    return 0;
  lua_pushlstring(L, val.dptr, val.dsize);
  return 1;
}

/* dbm:store(key, value) -> status {{{1
 *
 * Store the [dbm] [dbm] record @value (a string) by the given string @key. On
 * success true is returned, otherwise a nil followed by an error message is
 * returned.
 */

int dbm_store(lua_State *L)
{
  lua_apr_dbm *object;
  apr_datum_t key, val;
  apr_status_t status;

  object = dbm_check(L, 1, 1);
  datum_check(L, 2, &key);
  datum_check(L, 3, &val);
  status = apr_dbm_store(object->handle, key, val);

  return push_status(L, status);
}

/* dbm:delete(key) -> status {{{1
 *
 * Delete the [dbm] [dbm] record with the given string @key. On success true is
 * returned, otherwise a nil followed by an error message is returned.
 */

int dbm_delete(lua_State *L)
{
  lua_apr_dbm *object;
  apr_datum_t key;
  apr_status_t status;

  object = dbm_check(L, 1, 1);
  datum_check(L, 2, &key);
  status = apr_dbm_delete(object->handle, key);

  return push_status(L, status);
}

/* dbm:firstkey() -> key {{{1
 *
 * Retrieve the first record key from a [dbm] [dbm]. On success the first key
 * is returned as a string, when there are no keys nothing is returned. In case
 * of error a nil followed by an error message is returned.
 */

int dbm_firstkey(lua_State *L)
{
  lua_apr_dbm *object;
  apr_datum_t key;
  apr_status_t status;

  object = dbm_check(L, 1, 1);
  status = apr_dbm_firstkey(object->handle, &key);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);
  else if (key.dsize == 0)
    return 0;
  lua_pushlstring(L, key.dptr, key.dsize);

  return 1;
}

/* dbm:nextkey(key1) -> key2 {{{1
 *
 * Retrieve the next record key from a [dbm] [dbm]. This function works just
 * like Lua's `next()` function: On success the next key is returned as a
 * string, when there are no more keys nothing is returned. In case of error a
 * nil followed by an error message is returned.
 */

int dbm_nextkey(lua_State *L)
{
  lua_apr_dbm *object;
  apr_datum_t key;
  apr_status_t status;

  object = dbm_check(L, 1, 1);
  datum_check(L, 2, &key);
  status = apr_dbm_nextkey(object->handle, &key);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);
  else if (key.dsize == 0)
    return 0;
  lua_pushlstring(L, key.dptr, key.dsize);

  return 1;
}

/* dbm:close() -> status {{{1
 *
 * Close a [dbm] [dbm] database handle and return true (this can't fail).
 */

int dbm_close(lua_State *L)
{
  lua_apr_dbm *dbm;

  dbm = dbm_check(L, 1, 1);
  dbm_close_impl(L, dbm);
  lua_pushboolean(L, 1);

  return 1;
}

/* dbm:__tostring() {{{1 */

int dbm_tostring(lua_State *L)
{
  lua_apr_dbm *dbm;

  dbm = dbm_check(L, 1, 0);
  if (dbm->handle != NULL)
    lua_pushfstring(L, "%s (%p)", lua_apr_dbm_type.friendlyname, dbm->handle);
  else
    lua_pushfstring(L, "%s (closed)", lua_apr_dbm_type.friendlyname);

  return 1;
}

/* dbm:__gc() {{{1 */

int dbm_gc(lua_State *L)
{
  lua_apr_dbm *dbm = dbm_check(L, 1, 0);
  if (object_collectable((lua_apr_refobj*)dbm))
    dbm_close_impl(L, dbm);
  release_object((lua_apr_refobj*)dbm);
  return 0;
}

/* }}} */

luaL_reg dbm_methods[] = {
  { "close", dbm_close },
  { "delete", dbm_delete },
  { "exists", dbm_exists },
  { "fetch", dbm_fetch },
  { "firstkey", dbm_firstkey },
  { "nextkey", dbm_nextkey },
  { "store", dbm_store },
  { NULL, NULL },
};

luaL_reg dbm_metamethods[] = {
  { "__tostring", dbm_tostring },
  { "__eq", objects_equal },
  { "__gc", dbm_gc },
  { NULL, NULL },
};

lua_apr_objtype lua_apr_dbm_type = {
  "lua_apr_dbm*",      /* metatable name in registry */
  "dbm",               /* friendly object name */
  sizeof(lua_apr_dbm), /* structure size */
  dbm_methods,         /* methods table */
  dbm_metamethods      /* metamethods table */
};
