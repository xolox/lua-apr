/* Serialization module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: November 20, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * The Lua/APR binding contains a serialization function based on the [Metalua
 * table-to-source serializer] [metalua_serializer] extended to support
 * function upvalues and userdata objects created by the Lua/APR binding. The
 * following Lua values can be serialized:
 *
 *  - strings, numbers, booleans, nil (scalars)
 *  - Lua functions (including upvalues)
 *  - userdata objects created by the Lua/APR binding
 *  - tables thereof. There is no restriction on keys; recursive and shared
 *    sub-tables are handled correctly.
 *
 * Restrictions:
 *
 *  - *Metatables and environments aren't saved*;
 *    this might or might not be what you want.
 *
 *  - *If multiple functions share a scalar upvalue they will each get their
 *    own copy.* Because it is impossible to join upvalues of multiple
 *    functions in Lua 5.1 this won't be fixed any time soon.
 *
 * The following functions in the Lua/APR binding internally use serialization
 * to transfer Lua functions and other values between operating system threads:
 *
 *  - `apr.thread()` and `thread:wait()`
 *  - `queue:push()` and `queue:pop()`
 *  - `queue:try_push()` and `queue:try_pop()`
 *
 * [metalua_serializer]: https://github.com/fab13n/metalua/blob/master/src/lib/serialize.lua
 */

/* TODO Verify that we're reference counting correctly...
 * TODO Locking around modifications of chain?! Bad experiences with mutexes :-\
 */

#include "lua_apr.h"
#include <apr_uuid.h>

/* Internal stuff. {{{1 */

typedef struct reference reference;

struct reference {
  char uuid[APR_UUID_FORMATTED_LENGTH + 1];
  lua_apr_objtype *type;
  lua_apr_refobj *object;
  reference *next;
};

static reference *chain = NULL;

static void load_lua_apr(lua_State *L)
{
  /* Make sure the Lua/APR binding is loaded. */
  lua_getglobal(L, "require");
  lua_pushliteral(L, "apr");
  lua_call(L, 1, 1);
  /* require('apr') should raise on errors, but just as a sanity check: */
  if (!lua_istable(L, -1))
    raise_error_message(L, "Failed to load Lua/APR binding!");
}

/* apr.ref(object) -> uuid {{{1
 *
 * Prepare the Lua/APR userdata @object so that it can be referenced from
 * another Lua state in the same operating system process and associate a
 * [UUID] [uuid] with the object. The UUID is returned as a string. When you
 * pass this UUID to `apr.deref()` you'll get the same object back. This only
 * works once, but of course you're free to generate another UUID for the same
 * object.
 *
 * [uuid]: http://en.wikipedia.org/wiki/Universally_unique_identifier
 */

int lua_apr_ref(lua_State *L)
{
  lua_apr_objtype *type = NULL;
  reference *node = NULL;
  apr_uuid_t uuid;
  int i;

  /* Make sure we're dealing with a userdata object. */
  luaL_checktype(L, 1, LUA_TUSERDATA);

  /* Make sure the userdata has one of the supported types. */
  for (i = 0; lua_apr_types[i] != NULL; i++)
    if (object_has_type(L, 1, lua_apr_types[i], 1)) {
      type = lua_apr_types[i];
      break;
    }
  luaL_argcheck(L, type != NULL, 1, "userdata cannot be referenced");

  /* Prepare to insert object in chain of references. */
  node = calloc(1, sizeof(reference));
  if (node == NULL)
    raise_error_memory(L);
  node->object = prepare_reference(type, lua_touserdata(L, 1));
  if (node->object == NULL) {
    free(node);
    raise_error_memory(L);
  }
  node->type = type;
  apr_uuid_get(&uuid);
  apr_uuid_format(node->uuid, &uuid);

  /* Increase the reference count of the object because it is now being
   * referenced from the Lua state and the chain of references. */
  object_incref(node->object);

  /* Insert the object into the chain of references. */
  node->next = chain;
  chain = node;

  /* Return newly associated UUID for object. */
  lua_pushlstring(L, node->uuid, APR_UUID_FORMATTED_LENGTH);
  return 1;
}

/* apr.deref(uuid) -> object {{{1
 *
 * Convert a UUID that was previously returned by `apr.ref()` into a userdata
 * object and return the object. You can only dereference a UUID once, but of
 * course you're free to generate another UUID for the same object.
 */

int lua_apr_deref(lua_State *L)
{
  const char *uuid;
  reference *node, *last = NULL;

  uuid = luaL_checkstring(L, 1);

  /* Look for the UUID in the chain of object references. */
  node = chain;
  while (node != NULL) {
    if (node->object != NULL && strcmp(uuid, node->uuid) == 0) {
      /* Return an object that references the real object in unmanaged memory. */
      create_reference(L, node->type, node->object);
      /* Invalidate the UUID. */
      if (node == chain)
        chain = node->next;
      else
        last->next = node->next;
      free(node);
      return 1;
    }
    last = node;
    node = node->next;
  }

  luaL_argerror(L, 1, "userdata has not been referenced");

  /* Make the compiler happy. */
  return 0;
}

/* lua_apr_serialize() - serialize values from "idx" to stack top (pops 0..n values, pushes string) {{{1 */

int lua_apr_serialize(lua_State *L, int idx)
{
  int num_args = lua_gettop(L) - idx + 1; /* calculate number of arguments */
  load_lua_apr(L);                        /* load Lua/APR binding */
  lua_getfield(L, -1, "serialize");       /* get apr.serialize() function */
  if (!lua_isfunction(L, -1))             /* make sure we found it */
    raise_error_message(L, "Failed to load apr.serialize() function!");
  lua_insert(L, idx);                     /* move function before arguments */
  lua_pop(L, 1);                          /* remove "apr" table from stack */
  lua_call(L, num_args, 1);               /* call function (propagating errors upwards) */
  if (!lua_isstring(L, -1))               /* apr.serialize() should raise on errors, but just as a sanity check: */
    raise_error_message(L, "Failed to serialize value(s) using apr.serialize()");
  return 1;                               /* leave result string on top of stack */
}

/* lua_apr_unserialize() - unserialize string at top of stack (pops string, pushes 0..n values). {{{1 */

int lua_apr_unserialize(lua_State *L)
{
  int idx = lua_gettop(L);            /* remember input string stack index */
  load_lua_apr(L);                    /* load Lua/APR binding */
  lua_getfield(L, -1, "unserialize"); /* get apr.unserialize() function */
  if (!lua_isfunction(L, -1))         /* make sure we found it */
    raise_error_message(L, "Failed to load apr.unserialize() function!");
  lua_insert(L, idx);                 /* move function before input string */
  lua_pop(L, 1);                      /* pop "apr" table (now at top of stack) */
  lua_call(L, 1, LUA_MULTRET);        /* call function with string argument (propagating errors upwards) */
  return lua_gettop(L) - idx;         /* return 0..n unserialized values */
}

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
