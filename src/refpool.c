/* Reference counted APR memory pools for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: October 18, 2010
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 */

#include "lua_apr.h"

lua_apr_pool *refpool_alloc(lua_State *L) /* {{{1 */
{
  apr_status_t status;
  apr_pool_t *memory_pool;
  lua_apr_pool *refpool = NULL;

  status = apr_pool_create(&memory_pool, NULL);
  if (status != APR_SUCCESS)
    raise_error_status(L, status);

  refpool = apr_palloc(memory_pool, sizeof(*refpool));
  refpool->ptr = memory_pool;
  refpool->refs = 0;

  return refpool;
}

apr_pool_t* refpool_incref(lua_apr_pool *refpool) /* {{{1 */
{
  refpool->refs++;
  return refpool->ptr;
}

void refpool_decref(lua_apr_pool *refpool) /* {{{1 */
{
  refpool->refs--;
  if (refpool->refs == 0)
    apr_pool_destroy(refpool->ptr);
}
