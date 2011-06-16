/* Relational database drivers module for the Lua/APR binding.
 *
 * Authors:
 *  - zhiguo zhao <zhaozg@gmail.com>
 *  - Peter Odding <peter@peterodding.com>
 * Last Change: June 16, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * The APR DBD module makes it possible to query relational database engines
 * like [SQLite] [sqlite], [MySQL] [mysql], [PostgreSQL] [pgsql], [Oracle]
 * [oracle], [ODBC] [odbc] and [FreeTDS] [freetds]. This module currently has
 * some drawbacks which appear to be unavoidable given the design of the Apache
 * Portable Runtime DBD framework:
 *
 *  - Booleans and numbers are returned as strings because result set types are
 *    not known to the Lua/APR binding (and type guessing is too error prone)
 *
 *  - String arguments and results are not binary safe, this means they will
 *    be truncated at the first zero byte (see the [apr-dev mailing list]
 *    [apr_dev] thread ["current dbd initiatives"] [dbd_binary_discuss] for
 *    discussion about this limitation of APR)
 *
 * Note that if you control the data going into the database you can overcome
 * the second limitation by using APR's built in support for
 * [Base64 encoding](#base64_encoding).
 *
 * ### Installing drivers
 *
 * On Debian/Ubuntu Linux you can install one or more of the following packages
 * to enable support for the corresponding database driver (dependencies will
 * be installed automatically by the package management system):
 *
 *  - [libaprutil1-dbd-sqlite3][sqlite3_pkg]
 *  - [libaprutil1-dbd-mysql](http://packages.ubuntu.com/lucid/libaprutil1-dbd-mysql)
 *  - [libaprutil1-dbd-pgsql](http://packages.ubuntu.com/lucid/libaprutil1-dbd-pgsql)
 *  - [libaprutil1-dbd-odbc](http://packages.ubuntu.com/lucid/libaprutil1-dbd-odbc)
 *  - [libaprutil1-dbd-freetds](http://packages.ubuntu.com/lucid/libaprutil1-dbd-freetds)
 *
 * ### Debugging "DSO load failed" errors
 *
 * In my initial tests on Ubuntu I installed [libaprutil1-dbd-sqlite3]
 * [sqlite3_pkg] but kept getting an error when trying to load the driver:
 *
 *     $ lua -e "print(require('apr').dbd('sqlite3'))"
 *     nil  DSO load failed  EDSOOPEN
 *
 * After a while I found the problem using [LD_DEBUG] [ld_debug]:
 *
 *     $ LD_DEBUG=libs lua -e "require('apr').dbd('sqlite3')" 2>&1 | grep undefined
 *     /usr/lib/apr-util-1/apr_dbd_sqlite3-1.so: error: symbol lookup error: undefined symbol: apr_pool_cleanup_null (fatal)
 *
 * Having identified the problem, finding a workaround was easy:
 *
 *     $ export LD_PRELOAD='/usr/lib/libapr-1.so.0:/usr/lib/libaprutil-1.so.0'
 *     $ lua -e "print(require('apr').dbd('sqlite3'))"
 *     database driver (0x853bdfc)
 *
 * [sqlite]: http://en.wikipedia.org/wiki/SQLite
 * [mysql]: http://en.wikipedia.org/wiki/MySQL
 * [pgsql]: http://en.wikipedia.org/wiki/PostgreSQL
 * [oracle]: http://en.wikipedia.org/wiki/Oracle_Database
 * [odbc]: http://en.wikipedia.org/wiki/ODBC
 * [freetds]: http://en.wikipedia.org/wiki/FreeTDS
 * [apr_dev]: http://apr.apache.org/mailing-lists.html
 * [dbd_binary_discuss]: http://marc.info/?l=apr-dev&m=114969441721086&w=2
 * [sqlite3_pkg]: http://packages.ubuntu.com/lucid/libaprutil1-dbd-sqlite3
 * [ld_debug]: http://www.wlug.org.nz/LD_DEBUG
 */

#include "lua_apr.h"
#include <apr_dbd.h>

/* Lua object structures. {{{1 */

/* Driver objects. */
typedef struct {
  lua_apr_refobj header;
  int generation;
  apr_pool_t *pool;
  const apr_dbd_driver_t *driver;
  apr_dbd_t *handle;
  apr_dbd_transaction_t *trans;
} lua_apr_dbd_object;

/* XXX The result set object's memory pool must be the same memory pool used
 * for the driver object, otherwise the SQLite backend will error out on the
 * second SELECT query executed. I can't find any documentation on this and it
 * seems counterintuitive: why require a memory pool in the apr_dbd_select()
 * argument list when it must be the same memory pool as that of the driver
 * anyway?! (which is also in the argument list) */

/* Reference to driver object stored in result set or prepared statement. */
typedef struct {
  lua_apr_dbd_object *driver;
  int generation;
} lua_apr_dbd_reference;

/* Result set objects. */
typedef struct {
  lua_apr_refobj header;
  lua_apr_dbd_reference parent;
  apr_dbd_results_t *results;
  int random_access, rownum;
} lua_apr_dbr_object;

/* Prepared statement objects. */
typedef struct {
  lua_apr_refobj header;
  lua_apr_dbd_reference parent;
  apr_dbd_prepared_t *statement;
} lua_apr_dbp_object;

/* Internal functions. {{{1 */

/* check_dbd() {{{2 */

static lua_apr_dbd_object *check_dbd(lua_State *L, int idx, int connected, int transaction)
{
  lua_apr_dbd_object *object;

  object = check_object(L, idx, &lua_apr_dbd_type);
  if (connected && object->handle == NULL)
    luaL_error(L, "attempt to use a closed database driver");
  else if (transaction && object->trans == NULL)
    luaL_error(L, "this operation only works when a transaction is active");

  return object;
}

static void *check_subobj(lua_State *L, int idx, lua_apr_objtype *type, size_t offset) /* {{{2 */
{
  void *object;
  lua_apr_dbd_reference *reference;

  object = check_object(L, idx, type);
  if (object != NULL) {
    reference = (void*)((char*)object + offset);
    if (reference->generation != reference->driver->generation)
      luaL_error(L, "database driver has been reinitialized");
  }

  return object;
}

/* check_dbr() {{{2 */

static lua_apr_dbr_object *check_dbr(lua_State *L, int idx)
{
  return check_subobj(L, idx, &lua_apr_dbr_type, offsetof(lua_apr_dbr_object, parent));
}

/* check_dbp() {{{2 */

static lua_apr_dbp_object *check_dbp(lua_State *L, int idx)
{
  return check_subobj(L, idx, &lua_apr_dbp_type, offsetof(lua_apr_dbp_object, parent));
}

/* new_subobj() {{{2 */

static void *new_subobj(lua_State *L, lua_apr_objtype *type, int idx, lua_apr_dbd_object *driver, size_t offset)
{
  void *object = new_object(L, type);
  if (object != NULL) {
    lua_apr_dbd_reference *reference;
    reference = (void*)((char*)object + offset);
    reference->driver = driver;
    reference->generation = driver->generation;
    /* Let Lua's garbage collector know we want the driver to stay alive. */
    object_env_private(L, -1); /* get result set environment */
    lua_pushvalue(L, idx); /* copy driver object reference */
    lua_setfield(L, -2, "driver"); /* store reference to driver */
    lua_pop(L, 1); /* remove environment from stack */
  }
  return object;
}

/* new_resultset() {{{2 */

static lua_apr_dbr_object *new_resultset(lua_State *L, int idx, lua_apr_dbd_object *driver, int random_access)
{
  lua_apr_dbr_object *object;

  object = new_subobj(L, &lua_apr_dbr_type, idx, driver, offsetof(lua_apr_dbr_object, parent));
  if (object != NULL)
    object->random_access = random_access;

  return object;
}

/* new_statement() {{{2 */

static lua_apr_dbp_object *new_statement(lua_State *L, int idx, lua_apr_dbd_object *driver)
{
  return new_subobj(L, &lua_apr_dbp_type, idx, driver, offsetof(lua_apr_dbp_object, parent));
}

/* check_dbd_value() {{{2 */

static const char *check_dbd_value(lua_State *L, int idx, int origidx, int i)
{
  int type = lua_type(L, idx);
  if (type == LUA_TNIL || type == LUA_TNONE)
    return NULL;
  else if (type == LUA_TBOOLEAN)
    /* Not sure if this is standard but SQLite and MySQL do it like this. */
    return lua_toboolean(L, idx) ? "1" : "0";
  else if (type == LUA_TNUMBER || type == LUA_TSTRING)
    return lua_tostring(L, idx);
  else if (origidx == 0)
    /* Let the user know we were expecting a string argument. */
    luaL_checkstring(L, idx);
  else
    /* Let the user know which value in the table was invalid. */
    luaL_argcheck(L, 0, origidx, lua_pushfstring(L, "invalid value at index %d", i));
  return NULL;
}

/* check_dbd_values() {{{2 */

static void check_dbd_values(lua_State *L, int idx, int *nargs, const char ***values)
{
  /* I'm a three star programmer ;-) http://c2.com/cgi/wiki?ThreeStarProgrammer */
  int i, is_table = lua_istable(L, idx);
  *nargs = is_table ? lua_objlen(L, idx) : (lua_gettop(L) - idx + 1);
  *values = malloc(sizeof(**values) * *nargs);
  if (*nargs > 0 && *values == NULL)
    raise_error_memory(L);
  if (is_table) {
    for (i = 0; i < *nargs; i++) {
      lua_pushinteger(L, i + 1);
      lua_gettable(L, 2);
      (*values)[i] = check_dbd_value(L, -1, idx, i + 1);
      lua_pop(L, 1);
    }
  } else {
    for (i = 0; i < *nargs; i++)
      (*values)[i] = check_dbd_value(L, idx + i, 0, 0);
  }
}

/* push_dbd_value() {{{2 */

static int push_dbd_value(lua_State *L, const char *value)
{
  lua_pushstring(L, value);
  return 1;
}

/* push_dbd_error() {{{2 */

static int push_dbd_error(lua_State *L, lua_apr_dbd_object *object, int status)
{
  const char *message;

  message = apr_dbd_error(object->driver, object->handle, status);
  lua_pushnil(L);
  lua_pushstring(L, message);

  return 2;
}

/* push_dbd_status() {{{2 */

static int push_dbd_status(lua_State *L, lua_apr_dbd_object *object, int status)
{
  if (status != 0)
    return push_dbd_error(L, object, status);
  lua_pushboolean(L, 1);
  return 1;
}

/* dbd_error() {{{2 */

static int dbd_error(lua_State *L)
{
  const lua_apr_dbd_object *object = check_dbd(L, 1, 0, 0);
  int err = luaL_checkint(L, 2);

  lua_pushstring(L, apr_dbd_error(object->driver, object->handle, err));
  return 1;
}

/* dbd_escape() {{{2 */

static int dbd_escape(lua_State *L)
{
  const lua_apr_dbd_object *object = check_dbd(L, 1, 1, 0);
  const char *string = luaL_checkstring(L, 2);

  lua_pushstring(L, apr_dbd_escape(object->driver, object->pool, string,
        object->handle));
  return 1;
}

/* push_query_result() {{{2 */

static int push_query_result(lua_State *L, lua_apr_dbd_object *object, int nrows, int status)
{
  push_dbd_status(L, object, status);
  if (status == 0)
    lua_pushinteger(L, nrows);
  return 2;
}

/* next_record() {{{2 */

typedef int (*push_row_cb)(lua_State*, lua_apr_dbr_object*, apr_dbd_row_t*);

static int next_record(lua_State *L, push_row_cb push_row)
{
  lua_apr_dbd_object *driver;
  lua_apr_dbr_object *results;
  apr_dbd_row_t *row;
  int status, iterator, rownum;

  if (lua_isuserdata(L, lua_upvalueindex(1))) {
    iterator = 1;
    results = lua_touserdata(L, lua_upvalueindex(1));
    rownum = -1; /* iterators keep their own row numbers */
  } else {
    iterator = 0;
    results = check_dbr(L, 1);
    /* results->rownum is initialized to 0 by memset() */
    rownum = luaL_optint(L, 2, results->rownum + 1);
  }

  driver = results->parent.driver;
  status = apr_dbd_get_row(driver->driver, driver->pool, results->results,
      &row, rownum);

  if (status == -1) {
    /* rownum out of range or data finished */
    return 0;
  } else if (status != 0) {
    /* undocumented error? */
    push_dbd_error(L, driver, status);
    if (iterator)
      lua_error(L);
    return 2;
  } else {
    if (rownum != -1)
      results->rownum = rownum;
    else
      rownum = ++results->rownum;
    return push_row(L, results, row);
  }
}

/* dbr_tuple_cb() {{{2 */

static int dbr_tuple_cb(lua_State *L, lua_apr_dbr_object *results, apr_dbd_row_t *row)
{
  const char *value;
  int i = 0;
  while ((value = apr_dbd_get_entry(results->parent.driver->driver, row, i++)) != NULL)
    push_dbd_value(L, value);
  return i - 1;
}

/* dbr_row_cb() {{{2 */

static int dbr_row_cb(lua_State *L, lua_apr_dbr_object *results, apr_dbd_row_t *row)
{
  lua_apr_dbd_object *driver;
  int i = 0, is_pairs;
  const char *name;

  driver = results->parent.driver;
  is_pairs = lua_toboolean(L, lua_upvalueindex(2));
  if (is_pairs)
    lua_pushinteger(L, results->rownum);
  lua_newtable(L);
  while ((name = apr_dbd_get_name(driver->driver, results->results, i)) != NULL) {
    push_dbd_value(L, apr_dbd_get_entry(driver->driver, row, i++));
    lua_setfield(L, -2, name);
  }
  return is_pairs ? 2 : 1;
}

/* dbd_close_impl() {{{2 */

static apr_status_t dbd_close_impl(lua_apr_dbd_object *driver)
{
  apr_status_t status = APR_SUCCESS;
  if (driver->handle != NULL) {
    status = apr_dbd_close(driver->driver, driver->handle);
    if (status == APR_SUCCESS) {
      driver->handle = NULL;
      driver->generation++;
    }
  }
  return status;
}

/* apr.dbd(name) -> driver {{{1
 *
 * Create a database driver object. The string @name decides which database
 * engine to use. On success the driver object is returned, otherwise a nil
 * followed by an error message is returned. Currently supported engines
 * include:
 *
 *  - `'mysql'`
 *  - `'pgsql'`
 *  - `'sqlite3'`
 *  - `'sqlite2'`
 *  - `'oracle'`
 *  - `'freetds'`
 *  - `'odbc'`
 *
 * Note that in its default configuration the Apache Portable Runtime uses
 * dynamic loading to load database drivers which means `apr.dbd()` can fail
 * because a driver can't be loaded.
 */

int lua_apr_dbd(lua_State *L)
{
  static int ginit = 0;

  apr_status_t status;
  lua_apr_dbd_object *driver;
  apr_pool_t *pool;
  const char *name;

  pool = to_pool(L);
  name = luaL_checkstring(L, 1);

  if (ginit == 0) {
    status = apr_dbd_init(pool);
    if (status != APR_SUCCESS)
      return push_error_status(L, status);
    ginit++;
  }

  driver = new_object(L, &lua_apr_dbd_type);
  if (driver == NULL)
    return push_error_memory(L);
  /* FIXME I'm not sure whether these pools should be related! */
  status = apr_pool_create_ex(&driver->pool, pool, NULL, NULL);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);

  status = apr_dbd_get_driver(driver->pool, name, &driver->driver);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);

  return 1;
}

/* driver:open(params) -> status {{{1
 *
 * Open a connection to a backend. The string @params contains the arguments to
 * the driver (implementation-dependent). On success true is returned,
 * otherwise a nil followed by an error message is returned. The syntax of
 * @params is as follows:
 *
 *  - __PostgreSQL:__ @params is passed directly to the [PQconnectdb()]
 *    [pqconnectdb] function, check the PostgreSQL documentation for more
 *    details on the syntax
 *
 *  - __SQLite2:__ @params is split on a colon, with the first part used as the
 *    filename and the second part converted to an integer and used as the file
 *    mode
 *
 *  - __SQLite3:__ @params is passed directly to the [sqlite3_open()]
 *    [sqlite3_open] function as a filename to be opened, check the SQLite3
 *    documentation for more details (hint: if @params is `':memory:'` a
 *    private, temporary in-memory database is created for the connection)
 *
 *  - __Oracle:__ @params can have 'user', 'pass', 'dbname' and 'server' keys,
 *    each followed by an equal sign and a value. Such key/value pairs can be
 *    delimited by space, CR, LF, tab, semicolon, vertical bar or comma
 *
 *  - __MySQL:__ @params can have 'host', 'port', 'user', 'pass', 'dbname',
 *    'sock', 'flags' 'fldsz', 'group' and 'reconnect' keys, each followed by
 *    an equal sign and a value. Such key/value pairs can be delimited by
 *    space, CR, LF, tab, semicolon, vertical bar or comma. For now, 'flags'
 *    can only recognize `CLIENT_FOUND_ROWS` (check MySQL manual for details).
 *    The value associated with 'fldsz' determines maximum amount of memory (in
 *    bytes) for each of the fields in the result set of prepared statements.
 *    By default, this value is 1 MB. The value associated with 'group'
 *    determines which group from configuration file to use (see
 *    `MYSQL_READ_DEFAULT_GROUP` option of [mysql_options()] [mysql_options] in
 *    MySQL manual). Reconnect is set to 1 by default (i.e. true)
 *
 *  - __FreeTDS:__ @params can have 'username', 'password', 'appname',
 *    'dbname', 'host', 'charset', 'lang' and 'server' keys, each followed by
 *    an equal sign and a value
 *
 *  [pqconnectdb]: http://www.postgresql.org/docs/current/interactive/libpq-connect.html#LIBPQ-PQCONNECTDB
 *  [sqlite3_open]: http://www.sqlite.org/c3ref/open.html
 *  [mysql_options]: http://dev.mysql.com/doc/refman/5.5/en/mysql-options.html
 */

static int dbd_open(lua_State *L)
{
  lua_apr_dbd_object *driver;
  const char *params, *error;
  apr_status_t status;

  driver = check_dbd(L, 1, 0, 0);
  params = luaL_checkstring(L, 2);

  if (driver->handle != NULL) {
    /* XXX It's not clear whether apr_dbd_close() returns proper apr_status_t
     * values or not... Looking at the SQLite3 and MySQL backends any errors
     * are silently ignored?! */
    status = apr_dbd_close(driver->driver, driver->handle);
    if (status != APR_SUCCESS)
      return push_error_status(L, status);
    driver->generation++;
    driver->handle = NULL;
  }
  status = apr_dbd_open_ex(driver->driver, driver->pool, params,
      &driver->handle, &error);
  if (status != APR_SUCCESS) {
    lua_pushnil(L);
    lua_pushstring(L, error);
    return 2;
  }
  lua_pushboolean(L, 1);
  return 1;
}

/* driver:dbname(name) -> status {{{1
 *
 * Select the database @name. On succes true is returned, otherwise a nil
 * followed by an error message is returned. Not supported for all drivers
 * (e.g. SQLite by definition only knows a single database).
 */

static int dbd_dbname(lua_State *L)
{
  lua_apr_dbd_object *object;
  const char *name;
  int status;

  object = check_dbd(L, 1, 1, 0);
  name = luaL_checkstring(L, 2);
  status = apr_dbd_set_dbname(object->driver, object->pool, object->handle, name);

  return push_dbd_status(L, object, status);
}

/* driver:driver() -> name {{{1
 *
 * Get the name of the database driver. Returns one of the strings listed for
 * `apr.dbd()`.
 */

static int dbd_driver(lua_State *L)
{
  lua_apr_dbd_object *object = check_dbd(L, 1, 0, 0);
  lua_pushstring(L, apr_dbd_name(object->driver));
  return 1;
}

/* driver:check() -> status {{{1
 *
 * Check the status of the database connection. When the connection is still
 * alive true is returned, otherwise a nil followed by an error message is
 * returned.
 */

static int dbd_check_conn(lua_State *L)
{
  lua_apr_dbd_object *object;
  int status;

  object = check_dbd(L, 1, 1, 0);
  status = apr_dbd_check_conn(object->driver, object->pool, object->handle);

  return push_dbd_status(L, object, status);
}

/* driver:query(sql) -> status, affected_rows {{{1
 *
 * Execute an SQL query that doesn't return a result set. On success true
 * followed by the number of affected rows is returned, otherwise a nil
 * followed by an error message is returned.
 */

static int dbd_query(lua_State *L)
{
  lua_apr_dbd_object *object;
  const char *statement;
  int status, nrows = 0;

  object = check_dbd(L, 1, 1, 0);
  statement = luaL_checkstring(L, 2);
  status = apr_dbd_query(object->driver, object->handle, &nrows, statement);

  return push_query_result(L, object, nrows, status);
}

/* driver:select(sql [, random_access]) -> result_set {{{1
 *
 * Execute an SQL query that returns a result set. On success a result set
 * object is returned, otherwise a nil followed by an error message is
 * returned. To enable support for random access you can pass the optional
 * argument @random_access as true.
 */

static int dbd_select(lua_State *L)
{
  lua_apr_dbd_object *driver;
  lua_apr_dbr_object *results;
  const char *statement;
  int random_access, status;

  driver = check_dbd(L, 1, 1, 0);
  statement = luaL_checkstring(L, 2);
  random_access = lua_toboolean(L, 3);
  results = new_resultset(L, 1, driver, random_access);
  status = apr_dbd_select(driver->driver, driver->pool, driver->handle,
      &results->results, statement, random_access);
  if (status != 0)
    return push_dbd_error(L, driver, status);

  return 1;
}

/* Transactions. {{{1 */

/* driver:transaction_start() -> status {{{2
 *
 * Start a transaction. May be a no-op. On success true is returned, otherwise
 * a nil followed by an error message is returned.
 *
 * Note that transaction modes, set by calling `driver:transaction_mode()`,
 * will affect all query/select calls within a transaction. By default, any
 * error in query/select during a transaction will cause the transaction to
 * inherit the error code and any further query/select calls will fail
 * immediately. Put transaction in `'ignore-errors'` mode to avoid that. Use
 * `'rollback'` mode to do explicit rollback.
 */

static int dbd_transaction_start(lua_State *L)
{
  lua_apr_dbd_object *object;
  apr_status_t status;

  object = check_dbd(L, 1, 1, 0);
  status = apr_dbd_transaction_start(object->driver, object->pool,
      object->handle, &object->trans);

  return push_status(L, status);
}

/* driver:transaction_end() -> status {{{2
 *
 * End a transaction (commit on success, rollback on error). May be a no-op. On
 * success true is returned, otherwise a nil followed by an error message is
 * returned.
 */

static int dbd_transaction_end(lua_State *L)
{
  lua_apr_dbd_object *object;
  apr_status_t status;

  object = check_dbd(L, 1, 1, 1);
  status = apr_dbd_transaction_end(object->driver, object->pool, object->trans);

  return push_status(L, status);
}

/* driver:transaction_mode([mode]) -> mode {{{2
 *
 * Get or set the transaction mode, one of:
 *
 *  - `'commit'`: commit the transaction
 *  - `'rollback'`: rollback the transaction
 *  - `'ignore-errors'`: ignore transaction errors
 *
 * On success the new transaction mode is returned, otherwise a nil followed by
 * an error message is returned.
 */

static int dbd_transaction_mode(lua_State *L)
{
  const char *options[] = {
    "commit", "rollback", "ignore-errors", NULL };

  const int values[] = {
    APR_DBD_TRANSACTION_COMMIT,
    APR_DBD_TRANSACTION_ROLLBACK,
    APR_DBD_TRANSACTION_IGNORE_ERRORS };

  int i, mode;
  lua_apr_dbd_object *object;

  object = check_dbd(L, 1, 1, 1);
  if (!lua_isnoneornil(L, 2)) {
    mode = values[luaL_checkoption(L, 2, NULL, options)];
    mode = apr_dbd_transaction_mode_set(object->driver, object->trans, mode);
  } else {
    mode = apr_dbd_transaction_mode_get(object->driver, object->trans);
  }
  for (i = 0; i < count(values); i++)
    if (mode == values[i]) {
      lua_pushstring(L, options[i]);
      return 1;
    }
  assert(0);
  return 0;
}

/* Prepared statements. {{{1 */

/* driver:prepare(sql) -> prepared_statement {{{2
 *
 * Prepare a statement. On success a prepared statement object is returned,
 * otherwise a nil followed by an error message is returned.
 *
 * To specify parameters of the prepared query, use `%s`, `%d` etc. (see below
 * for full list) in place of database specific parameter syntax (e.g. for
 * PostgreSQL, this would be `$1`, `$2`, for SQLite3 this would be `?`, etc.).
 * For instance: `SELECT name FROM customers WHERE name = %s` would be a query
 * that this function understands. Here is the list of supported format
 * specifiers and what they map to in SQL (generally you'll only need the types
 * marked in bold text):
 *
 *  - `%hhd`: TINY INT
 *  - `%hhu`: UNSIGNED TINY INT
 *  - `%hd`: SHORT
 *  - `%hu`: UNSIGNED SHORT
 *  - __`%d`: INT__
 *  - __`%u`: UNSIGNED INT__
 *  - `%ld`: LONG
 *  - `%lu`: UNSIGNED LONG
 *  - `%lld`: LONG LONG
 *  - `%llu`: UNSIGNED LONG LONG
 *  - __`%f`: FLOAT, REAL__
 *  - __`%lf`: DOUBLE PRECISION__
 *  - __`%s`: VARCHAR__
 *  - __`%pDt`: TEXT__
 *  - `%pDi`: TIME
 *  - `%pDd`: DATE
 *  - `%pDa`: DATETIME
 *  - `%pDs`: TIMESTAMP
 *  - `%pDz`: TIMESTAMP WITH TIME ZONE
 *  - `%pDn`: NULL
 */

static int dbd_prepare(lua_State *L)
{
  lua_apr_dbd_object *driver;
  lua_apr_dbp_object *statement;
  const char *query;
  apr_status_t status;

  driver = check_dbd(L, 1, 1, 0);
  query = luaL_checkstring(L, 2);
  statement = new_statement(L, 1, driver);
  status = apr_dbd_prepare(driver->driver, driver->pool, driver->handle,
      query, NULL, &statement->statement);
  if (status != APR_SUCCESS)
    return push_dbd_error(L, driver, status);

  return 1;
}

/* prepared_statement:query(...) -> status {{{2
 *
 * Using a prepared statement, execute an SQL query that doesn't return a
 * result set. On success true followed by the number of affected rows is
 * returned, otherwise a nil followed by an error message is returned.
 *
 * If you pass a list then the values in the list become query parameters,
 * otherwise all function arguments become query parameters.
 *
 * *This function is not binary safe.*
 */

static int dbp_query(lua_State *L)
{
  lua_apr_dbd_object *driver;
  lua_apr_dbp_object *statement;
  int nargs, status, nrows = 0;
  const char **values;

  statement = check_dbp(L, 1);
  driver = statement->parent.driver;
  check_dbd_values(L, 2, &nargs, &values);
  status = apr_dbd_pquery(driver->driver, driver->pool, driver->handle, &nrows,
      statement->statement, nargs, values);
  free((void*) values); /* make MSVC++ 2010 happy */

  return push_query_result(L, driver, nrows, status);
}

/* prepared_statement:select(random_access, ...) -> result_set {{{2
 *
 * Using a prepared statement, execute an SQL query that returns a result set.
 * On success a result set object is returned, otherwise a nil followed by an
 * error message is returned. To enable support for random access pass
 * @random_access as true, otherwise pass it as false.
 *
 * If you pass a list then the values in the list become query parameters,
 * otherwise all function arguments after @random_access become query
 * parameters.
 *
 * TODO Make the @random_access argument an attribute of the prepared
 * statement object so that we can get rid of the argument here?!
 *
 * *This function is not binary safe.*
 */

static int dbp_select(lua_State *L)
{
  lua_apr_dbd_object *driver;
  lua_apr_dbp_object *statement;
  lua_apr_dbr_object *results;
  const char **values;
  int nargs, status, random_access;

  statement = check_dbp(L, 1);
  random_access = lua_toboolean(L, 2);
  driver = statement->parent.driver;
  check_dbd_values(L, 3, &nargs, &values);
  results = new_resultset(L, 1, driver, random_access);
  status = apr_dbd_pselect(driver->driver, driver->pool, driver->handle,
      &results->results, statement->statement, random_access, nargs, values);
  free((void*) values); /* make MSVC++ 2010 happy */
  if (status != 0)
    return push_dbd_error(L, driver, status);

  return 1;
}

/* tostring(prepared_statement) -> string {{{2 */

static int dbp_tostring(lua_State *L)
{
  lua_apr_dbp_object *object;

  object = check_dbp(L, 1);
  lua_pushfstring(L, "%s (%p)", lua_apr_dbp_type.friendlyname, object);

  return 1;
}

/* prepared_statement:__gc() {{{2 */

static int dbp_gc(lua_State *L)
{
  lua_apr_dbp_object *object = check_dbp(L, 1);
  release_object((lua_apr_refobj*)object);
  return 0;
}

/* Result sets. {{{1 */

/* result_set:columns([num]) -> name [, ...] {{{2
 *
 * If no arguments are given return the names of all columns in the result set,
 * otherwise return the name of the column with index @num (counting from one).
 * For example:
 *
 *     > driver = assert(apr.dbd 'sqlite3')
 *     > assert(driver:open ':memory:')
 *     > results = assert(driver:select [[ SELECT 1 AS col1, 2 AS col2 ]])
 *     > = assert(results:columns())
 *     { 'col1', 'col2' }
 */

static int dbr_columns(lua_State *L)
{
  lua_apr_dbd_object *driver;
  lua_apr_dbr_object *results;
  const char *name;
  int i = 0;

  results = check_dbr(L, 1);
  driver = results->parent.driver;

  if (lua_isnoneornil(L, 2)) {
    while ((name = apr_dbd_get_name(driver->driver, results->results, i++)) != NULL)
      lua_pushstring(L, name);
    return i - 1;
  } else {
    i = luaL_checkint(L, 2) - 1;
    name = apr_dbd_get_name(driver->driver, results->results, i);
    if (name != NULL) {
      lua_pushstring(L, name);
      return 1;
    }
    return 0;
  }
}

/* result_set:row(num) -> row {{{2
 *
 * Return a table with named fields for the next row in the result set or the
 * row with index @rownum if given. When there are no more rows nothing is
 * returned, in case of an error a nil followed by an error message is
 * returned.
 *
 * *This function is not binary safe.*
 */

static int dbr_row(lua_State *L)
{
  return next_record(L, dbr_row_cb);
}

/* result_set:rows() -> iterator {{{2
 *
 * Return an iterator that produces a table with named fields for each
 * (remaining) row in the result set.
 *
 * In Lua 5.2 you can also use `pairs(result_set)`.
 *
 * *This function is not binary safe.*
 */

static int dbr_rows(lua_State *L)
{
  check_dbr(L, 1);
  lua_settop(L, 1);
  lua_pushcclosure(L, dbr_row, 1);
  lua_insert(L, 1);
  return 1;
}

/* result_set:tuple([rownum]) -> value [, ...] {{{2
 *
 * Return a tuple for the next row in the result set or the row with index
 * @rownum if given. If more than one value is returned, the return values will
 * be in the same order as the column list in the SQL query. When there are no
 * more rows nothing is returned, in case of an error a nil followed by an
 * error message is returned.
 *
 * *This function is not binary safe.*
 */

static int dbr_tuple(lua_State *L)
{
  return next_record(L, dbr_tuple_cb);
}

/* result_set:tuples() -> iterator {{{2
 *
 * Return an iterator that produces a tuple for each (remaining) row in the
 * result set. The tuples produced by the iterator are in the same order as the
 * column list in the SQL query, for example:
 *
 *     > driver = assert(apr.dbd 'sqlite3')
 *     > assert(driver:open 'quotes.sqlite3')
 *     > results = assert(driver:select [[ SELECT author, quote FROM quotes ]])
 *     > for author, quote in results:tuples() do
 *     >>  print(author, 'wrote:')
 *     >>  print(quote)
 *     >>  print()
 *     >> end
 *
 * *This function is not binary safe.*
 */

static int dbr_tuples(lua_State *L)
{
  check_dbr(L, 1);
  lua_settop(L, 1);
  lua_pushcclosure(L, dbr_tuple, 1);
  lua_insert(L, 1);
  return 1;
}

/* result_set:pairs() -> iterator {{{2
 *
 * Return an iterator that produces a row number and a table with named fields
 * for each (remaining) row in the result set.
 *
 * In Lua 5.2 you can also use `ipairs(result_set)`.
 *
 * *This function is not binary safe.*
 */

static int dbr_pairs(lua_State *L)
{
  check_dbr(L, 1);
  lua_settop(L, 1);
  lua_pushboolean(L, 2);
  lua_pushcclosure(L, dbr_row, 2);
  lua_insert(L, 1);
  return 1;
}

/* #result_set -> num_tuples {{{2
 *
 * Get the number of rows in a result set of a synchronous select. If the
 * results are asynchronous -1 is returned.
 */

static int dbr_len(lua_State *L)
{
  lua_apr_dbd_object *driver;
  lua_apr_dbr_object *results;
  int num_tuples;

  results = check_dbr(L, 1);
  driver = results->parent.driver;
  num_tuples = apr_dbd_num_tuples(driver->driver, results->results);
  lua_pushinteger(L, num_tuples);

  return 1;
}

/* tostring(result_set) -> string {{{2 */

static int dbr_tostring(lua_State *L)
{
  lua_apr_dbr_object *object;

  object = check_dbr(L, 1);
  lua_pushfstring(L, "%s (%p)", lua_apr_dbr_type.friendlyname, object);

  return 1;
}

/* result_set:__gc() {{{2 */

static int dbr_gc(lua_State *L)
{
  lua_apr_dbr_object *object = check_dbr(L, 1);
  release_object((lua_apr_refobj*)object);
  return 0;
}

/* driver:close() -> status {{{1
 *
 * Close a connection to a backend.
 */

static int dbd_close(lua_State *L)
{
  lua_apr_dbd_object *object;
  apr_status_t status;

  object = check_dbd(L, 1, 1, 0);
  status = dbd_close_impl(object);

  return push_status(L, status);
}

/* tostring(driver) -> string {{{1 */

static int dbd_tostring(lua_State *L)
{
  lua_apr_dbd_object *object;

  object = check_dbd(L, 1, 0, 0);
  if (object->handle != NULL)
    lua_pushfstring(L, "%s (%p)", lua_apr_dbd_type.friendlyname, object);
  else
    lua_pushfstring(L, "%s (closed)", lua_apr_dbd_type.friendlyname);

  return 1;
}

/* driver:__gc() {{{1 */

static int dbd_gc(lua_State *L)
{
  lua_apr_dbd_object *driver = check_dbd(L, 1, 0, 0);
  if (object_collectable((lua_apr_refobj*)driver))
    dbd_close_impl(driver);
  release_object((lua_apr_refobj*)driver);
  return 0;
}

/* Internal object definitions. {{{1 */

/* Database driver objects. {{{2 */

static luaL_reg dbd_metamethods[] = {
  { "__tostring", dbd_tostring },
  { "__eq", objects_equal },
  { "__gc", dbd_gc },
  { NULL, NULL }
};

static luaL_reg dbd_methods[] = {
  /* Generic methods. */
  { "open", dbd_open },
  { "dbname", dbd_dbname },
  { "driver", dbd_driver },
  { "check", dbd_check_conn },
  { "query", dbd_query },
  { "select", dbd_select },
  { "prepare", dbd_prepare },
  { "error", dbd_error },
  { "escape", dbd_escape },
  { "close", dbd_close },
  { "transaction_start", dbd_transaction_start },
  { "transaction_end", dbd_transaction_end },
  { "transaction_mode", dbd_transaction_mode },
  { NULL, NULL }
};

lua_apr_objtype lua_apr_dbd_type = {
  "lua_apr_dbd_object*",      /* metatable name in registry */
  "database driver",          /* friendly object name */
  sizeof(lua_apr_dbd_object), /* structure size */
  dbd_methods,                /* methods table */
  dbd_metamethods             /* metamethods table */
};

/* Result set objects. {{{2 */

static luaL_reg dbr_metamethods[] = {
  { "__len", dbr_len },
  { "__tostring", dbr_tostring },
  { "__eq", objects_equal },
  { "__gc", dbr_gc },
#if LUA_VERSION_NUM >= 502
  { "__pairs", dbr_rows },
  { "__ipairs", dbr_pairs },
#endif
  { NULL, NULL }
};

static luaL_reg dbr_methods[] = {
  { "columns", dbr_columns },
  { "tuple", dbr_tuple },
  { "tuples", dbr_tuples },
  { "row", dbr_row },
  { "rows", dbr_rows },
  { "pairs", dbr_pairs },
  { NULL, NULL }
};

lua_apr_objtype lua_apr_dbr_type = {
  "lua_apr_dbr_object*",      /* metatable name in registry */
  "result set",               /* friendly object name */
  sizeof(lua_apr_dbr_object), /* structure size */
  dbr_methods,                /* methods table */
  dbr_metamethods             /* metamethods table */
};

/* Prepared statement objects. {{{2 */

static luaL_reg dbp_metamethods[] = {
  { "__tostring", dbp_tostring },
  { "__eq", objects_equal },
  { "__gc", dbp_gc },
  { NULL, NULL }
};

static luaL_reg dbp_methods[] = {
  { "query", dbp_query },
  { "select", dbp_select },
  { NULL, NULL }
};

lua_apr_objtype lua_apr_dbp_type = {
  "lua_apr_dbp_object*",      /* metatable name in registry */
  "prepared statement",       /* friendly object name */
  sizeof(lua_apr_dbp_object), /* structure size */
  dbp_methods,                /* methods table */
  dbp_metamethods             /* metamethods table */
};

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
