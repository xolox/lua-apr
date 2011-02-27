/* Command argument parsing module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: February 25, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 */

#include "lua_apr.h"
#include <apr_getopt.h>

int lua_apr_getopt(lua_State *L)
{
  apr_status_t status;
  apr_pool_t *pool;
  apr_getopt_t *os;
  const char *s, **argv;
  apr_getopt_option_t *opts;
  int i, j, argc, optc, silent;

  /* XXX No data validation because only apr.lua calls us directly! */

  silent = lua_toboolean(L, 3);
  lua_settop(L, 2);

  status = apr_pool_create(&pool, NULL);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);

  /* Copy options to array of apr_getopt_option_t structures. */
  optc = lua_objlen(L, 1);
  opts = apr_palloc(pool, sizeof opts[0] * (optc + 1));
  for (i = 1; i <= optc; i++) {
    lua_rawgeti(L, 1, i);
    lua_getfield(L, -1, "optch");
    s = lua_tostring(L, -1);
    opts[i - 1].optch = s && s[0] ? s[0] : 256;
    lua_pop(L, 1);
    lua_getfield(L, -1, "name");
    opts[i - 1].name = lua_tostring(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, -1, "has_arg");
    opts[i - 1].has_arg = lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, -1, "description");
    opts[i - 1].description = lua_tostring(L, -1);
    lua_pop(L, 2);
  }
  /* Terminate the options array. */
  opts[optc].optch = 0;

  /* Copy arguments to array of strings. */
  argc = lua_objlen(L, 2) + 1;
  argv = apr_palloc(pool, sizeof argv[0] * argc);
  for (i = 0; i <= argc; i++) {
    lua_rawgeti(L, 2, i);
    argv[i] = lua_tostring(L, -1);
    lua_pop(L, 1);
  }

  /* Initialize arguments for parsing by apr_getopt_long(). */
  status = apr_getopt_init(&os, pool, argc, argv);
  if (status != APR_SUCCESS)
    goto fail;
  os->interleave = 1;
  if (silent)
    os->errfn = NULL;

  /* Parse options, save matched options in table #1. */
  lua_createtable(L, 0, optc);
  for (;;) {
    char buffer[2] = { '\0', '\0' };
    i = 256;
    s = NULL;
    status = apr_getopt_long(os, opts,  &i, &s);
    if (status == APR_EOF)
      break;
    else if (status != APR_SUCCESS)
      goto fail;
    assert(i != 256);
    buffer[0] = i;
    /* Get existing value(s) for option. */
    lua_getfield(L, -1, buffer);
    if (s == NULL) {
      /* Options without arguments are counted. */
      if (!lua_isnumber(L, -1))
        lua_pushinteger(L, 1);
      else
        lua_pushinteger(L, lua_tointeger(L, -1) + 1);
      lua_setfield(L, -3, buffer);
      lua_pop(L, 1); /* existing value */
    } else if (lua_istable(L, -1)) {
      /* Add argument to table of existing values. */
      push_string_or_true(L, s);
      lua_rawseti(L, -2, lua_objlen(L, -2) + 1);
      lua_pop(L, 1); /* existing value */
    } else if (!lua_isnil(L, -1)) {
      /* Combine 1st and 2nd argument in table. */
      lua_newtable(L);
      lua_insert(L, -2);
      lua_rawseti(L, -2, 1);
      push_string_or_true(L, s);
      lua_rawseti(L, -2, 2);
      lua_setfield(L, -2, buffer);
    } else {
      /* Set 1st argument value. */
      lua_pop(L, 1); /* pop nil result */
      push_string_or_true(L, s);
      lua_setfield(L, -2, buffer);
    }
  }

  /* Save matched arguments in table #2. */
  lua_createtable(L, argc - os->ind, 0);
  for (i = 1, j = os->ind; j < argc; i++, j++) {
    lua_pushstring(L, os->argv[j]);
    lua_rawseti(L, -2, i);
  }

  /* Destroy the memory pool and return the two tables. */
  apr_pool_destroy(pool);
  return 2;

fail:
  apr_pool_destroy(pool);
  return push_error_status(L, status);
}
