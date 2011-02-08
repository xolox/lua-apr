/* User/group identification module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: September 25, 2010
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 */

#include "lua_apr.h"
#include <apr_user.h>

/* apr.user_get() -> username, groupname {{{1
 *
 * Get the username and groupname of the calling process. On success the
 * username and groupname are returned, otherwise a nil followed by an error
 * message is returned.
 */

int lua_apr_user_get(lua_State *L)
{
  apr_status_t status;
  apr_pool_t *pool;
  apr_uid_t uid;
  apr_gid_t gid;
  char *username, *groupname;

# if APR_HAS_USER
  pool = to_pool(L);
  status = apr_uid_current(&uid, &gid, pool);
  if (APR_SUCCESS == status)
    status = apr_uid_name_get(&username, uid, pool);
  if (APR_SUCCESS == status)
    status = apr_gid_name_get(&groupname, gid, pool);
  if (APR_SUCCESS != status)
    return push_error_status(L, status);
  lua_pushstring(L, username);
  lua_pushstring(L, groupname);
  return 2;
# else
  raise_error_status(L, APR_ENOTIMPL);
  return 0;
# endif
}

/* apr.user_homepath_get(username) -> homepath {{{1
 *
 * Get the [home directory] [home] for the named user. On success the directory
 * pathname is returned, otherwise a nil followed by an error message is
 * returned.
 *
 * [home]: http://en.wikipedia.org/wiki/Home_directory
 */

int lua_apr_user_homepath_get(lua_State *L)
{
  apr_status_t status;
  apr_pool_t *pool;
  const char *name;
  char *path;

# if APR_HAS_USER
  pool = to_pool(L);
  name = luaL_checkstring(L, 1);
  status = apr_uid_homepath_get(&path, name, pool);
  if (APR_SUCCESS != status)
    return push_error_status(L, status);
  lua_pushstring(L, path);
  return 1;
# else
  raise_error_status(L, APR_ENOTIMPL);
  return 0;
# endif
}

/* Push name for userid */

int push_username(lua_State *L, apr_pool_t *pool, apr_uid_t uid)
{
  char *username;

# if APR_HAS_USER
  if (APR_SUCCESS == apr_uid_name_get(&username, uid, pool))
    lua_pushstring(L, username);
  else
    lua_pushnil(L);
# else
  lua_pushnil(L);
# endif

  return 1;
}

/* Push name for groupid */

int push_groupname(lua_State *L, apr_pool_t *pool, apr_gid_t gid)
{
  char *groupname;

# if APR_HAS_USER
  if (APR_SUCCESS == apr_gid_name_get(&groupname, gid, pool))
    lua_pushstring(L, groupname);
  else
    lua_pushnil(L);
# else
  lua_pushnil(L);
# endif

  return 1;
}

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
