/* File status querying (stat()) for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: December 28, 2010
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 */

#include "lua_apr.h"
#include <apr_lib.h>
#include <apr_strings.h>

/* Lua/APR introduces one new field to the apr_stat() interface, which is the
 * filepath. It's defined to 0 so it won't influence the .wanted bitfield :) */

#define APR_FINFO_PATH 0

static const char *const options[] = {
  "atime", "csize", "ctime", "dev", "group", "inode", "link", "mtime",
  "name", "nlink", "path", "size", "type", "protection", "user", NULL
};

static const apr_int32_t flags[] = {
  APR_FINFO_ATIME, APR_FINFO_CSIZE, APR_FINFO_CTIME, APR_FINFO_DEV,
  APR_FINFO_GROUP, APR_FINFO_INODE, APR_FINFO_LINK, APR_FINFO_MTIME,
  APR_FINFO_NAME, APR_FINFO_NLINK, APR_FINFO_PATH, APR_FINFO_SIZE,
  APR_FINFO_TYPE, APR_FINFO_UPROT | APR_FINFO_GPROT | APR_FINFO_WPROT,
  APR_FINFO_USER
};

void push_stat_field(lua_State*, apr_finfo_t*, apr_int32_t, const char*);

void check_stat_request(lua_State *L, lua_apr_stat_context *ctx)
{
  apr_int32_t flag;
  int i;

  ctx->count = 0;
  ctx->wanted = 0;

  for (i = ctx->firstarg; i - ctx->firstarg <= count(flags) && !lua_isnoneornil(L, i) && i <= ctx->lastarg; i++) {
    ctx->wanted |= flag = flags[luaL_checkoption(L, i, NULL, options)];
    if (APR_FINFO_LINK != flag)
      ctx->fields[ctx->count++] = flag;
  }

  if (ctx->count == 0)
    for (i = 0; i < count(flags); ++i)
      ctx->wanted |= flags[i];
}

int push_stat_results(lua_State *L, lua_apr_stat_context *ctx, const char *path)
{
  int i;

  if (0 == ctx->count) {
    lua_createtable(L, 0, ctx->count);
    for (i = 0; i < count(flags); i++) {
      if (APR_FINFO_LINK != flags[i] && (ctx->info.valid & flags[i]) == flags[i]) {
        lua_pushstring(L, options[i]);
        push_stat_field(L, &ctx->info, flags[i], path);
        lua_rawset(L, -3);
      }
    }
    return 1;
  } else {
    for (i = 0; i < ctx->count; i++) {
      if ((ctx->info.valid & ctx->fields[i]) == ctx->fields[i]) {
        push_stat_field(L, &ctx->info, ctx->fields[i], path);
      } else {
        lua_pushboolean(L, 0);
      }
    }
    return ctx->count;
  }
}

void push_stat_field(lua_State *L, apr_finfo_t *info, apr_int32_t which, const char *path)
{
  switch (which) {

    /* user/group name */
    case APR_FINFO_USER:
      if (!push_username(L, info->pool, info->user))
        lua_pushnil(L);
      break;
    case APR_FINFO_GROUP:
      push_groupname(L, info->pool, info->group);
      break;

    /* dates */
    case APR_FINFO_CTIME:
      time_push(L, info->ctime);
      break;
    case APR_FINFO_MTIME:
      time_push(L, info->mtime);
      break;
    case APR_FINFO_ATIME:
      time_push(L, info->atime);
      break;

    /* numbers */
    case APR_FINFO_SIZE:
      lua_pushinteger(L, (lua_Integer) info->size);
      break;
    case APR_FINFO_CSIZE:
      lua_pushinteger(L, (lua_Integer) info->csize);
      break;
    case APR_FINFO_INODE:
      lua_pushinteger(L, (lua_Integer) info->inode);
      break;
    case APR_FINFO_NLINK:
      lua_pushinteger(L, (lua_Integer) info->nlink);
      break;
    case APR_FINFO_DEV:
      lua_pushinteger(L, (lua_Integer) info->device);
      break;

    /* file path / name */
    case APR_FINFO_PATH:
      if (path && 0 != strcmp(path, ".")) {
        char *filepath;
        apr_status_t status;
        status = apr_filepath_merge(&filepath, path, info->name, 0, info->pool);
        if (APR_SUCCESS == status) {
          lua_pushstring(L, filepath);
          break;
        }
      }

    /* fall through */
    case APR_FINFO_NAME:
      lua_pushstring(L, info->name);
      break;

    /* type */
    case APR_FINFO_TYPE:
      switch (info->filetype) {
        case APR_LNK:
          lua_pushliteral(L, "link");
          break;
        case APR_REG:
          lua_pushliteral(L, "file");
          break;
        case APR_PIPE:
          lua_pushliteral(L, "pipe");
          break;
        case APR_SOCK:
          lua_pushliteral(L, "socket");
          break;
        case APR_DIR:
          lua_pushliteral(L, "directory");
          break;
        case APR_BLK:
          lua_pushliteral(L, "block device");
          break;
        case APR_CHR:
          lua_pushliteral(L, "character device");
          break;
        default:
          lua_pushliteral(L, "unknown");
          break;
      }
      break;

    /* protection tables */
    case APR_FINFO_UPROT | APR_FINFO_GPROT | APR_FINFO_WPROT:
      push_protection(L, info->protection);
      break;

    /* Error in Lua/APR :( */
    default:
      assert(0);
      lua_pushnil(L);
  }
}

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
