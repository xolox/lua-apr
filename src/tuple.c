/* Inter thread serialization module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: June 30, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * Note the use of memcpy() to avoid unaligned access.
 * Relevant: http://lwn.net/Articles/259732/
 */

#include "lua_apr.h"

enum { TV_NIL, TV_FALSE, TV_TRUE, TV_NUMBER, TV_STRING, TV_OBJECT };

typedef unsigned char packed_type;

/* (static) check_value_size(L, idx, trusted) {{{1 */

static size_t check_value_size(lua_State *L, int idx, int trusted)
{
  switch (lua_type(L, idx)) {
    case LUA_TNIL:
    case LUA_TNONE:
    case LUA_TBOOLEAN:
      return sizeof(packed_type);
    case LUA_TNUMBER:
      return sizeof(packed_type) + sizeof(lua_Number);
    case LUA_TSTRING:
      return sizeof(packed_type) + sizeof(size_t) + lua_objlen(L, idx);
    case LUA_TUSERDATA:
      if (!trusted) {
        int i;
        lua_getmetatable(L, idx);
        for (i = 0; lua_apr_types[i] != NULL; i++)
          if (object_has_type(L, idx, lua_apr_types[i], 0)) {
            trusted = 1;
            break;
          }
        lua_pop(L, 1);
      }
      if (trusted)
        return sizeof(packed_type) + sizeof(lua_apr_objtype*) + sizeof(void*);
      break;
  }
  luaL_argerror(L, idx,
      lua_pushfstring(L,
          "failed to serialize unknown %s value",
          luaL_typename(L, idx)));
  return 0; /* make the compiler happy. */
}

/* (static) check_tuple_size(L, idx) {{{1 */

static size_t check_tuple_size(lua_State *L, int firstidx, int lastidx)
{
  size_t size, total;
  int i;

  total = sizeof(size_t);
  for (i = firstidx; i <= lastidx; i++) {
    size = check_value_size(L, i, 0);
    if (size == 0)
      break;
    total += size;
  }

  return total;
}

/* (static) check_value(L, idx, value) {{{1 */

static int check_value(lua_State *L, int idx, void *value)
{
  char *p = (char*) value;

  switch (lua_type(L, idx)) {
    case LUA_TNIL:
    case LUA_TNONE:
      *(packed_type*)value = TV_NIL;
      return 1;
    case LUA_TBOOLEAN:
      *(packed_type*)value = lua_toboolean(L, idx) ? TV_TRUE : TV_FALSE;
      return 1;
    case LUA_TNUMBER:
    {
      lua_Number n = lua_tonumber(L, idx);
      *(packed_type*)value = TV_NUMBER;
      memcpy(p + sizeof(packed_type), &n, sizeof n);
      return 1;
    }
    case LUA_TSTRING: {
      size_t size;
      const char *data = lua_tolstring(L, idx, &size);
      *(packed_type*)value = TV_STRING;
      memcpy(p + sizeof(packed_type), &size, sizeof size);
      memcpy(p + sizeof(packed_type) + sizeof size, data, size);
      return 1;
    }
    case LUA_TUSERDATA: {
      /* TODO Avoid this second loop using lua_apr_objtype[] array on stack. */
      int i;
      for (i = 0; lua_apr_types[i] != NULL; i++) {
        lua_apr_objtype *type = lua_apr_types[i];
        if (object_has_type(L, idx, type, 1)) {
          lua_apr_refobj *object = lua_touserdata(L, idx);
          void *reference = prepare_reference(type, object);
          *(packed_type*)value = TV_OBJECT;
          memcpy(p + sizeof(packed_type), &type, sizeof type);
          memcpy(p + sizeof(packed_type) + sizeof type, &reference, sizeof reference);
          /* XXX Increase the reference count assuming the tuple will be
           * unpacked eventually (barring exceptional errors?!) */
          object_incref(object);
          return 1;
        }
      }
    }
  }
  luaL_argerror(L, idx,
      lua_pushfstring(L,
          "failed to pack unknown %s value",
          luaL_typename(L, idx)));
  return 0; /* make the compiler happy. */
}

/* check_tuple(L, firstidx, lastidx, ptr) {{{1 */

int check_tuple(lua_State *L, int firstidx, int lastidx, void **ptr)
{
  void *tuple;
  size_t size;
  char *p;
  int i;

  if (lua_gettop(L) < lastidx)
    lua_settop(L, lastidx);
  size = check_tuple_size(L, firstidx, lastidx);
  p = tuple = *ptr = malloc(size);
  if (tuple == NULL)
    return 0;

  /* We start with a tuple of zero values. Reminder to myself: malloc() returns
   * 8 byte aligned pointers so this should be fine. */
  *(size_t*)tuple = 0;
  p += sizeof(size_t);

  for (i = firstidx; i <= lastidx; i++) {
    check_value(L, i, (void*)p);
    p += check_value_size(L, i, 1);
    (*(size_t*)tuple)++; /* increase size of tuple with one value */
  }

  return 1;
}

/* push_tuple(L, tuple) {{{1 */

int push_tuple(lua_State *L, void *tuple)
{
  char *ptr, *start;
  unsigned int i;

  ptr = start = (char*)tuple;
  ptr += sizeof(size_t);

  for (i = 0; i < *(size_t*)tuple; i++) {
    packed_type type = *(packed_type*)ptr;
    switch (type) {
      case TV_NIL:
        ptr += sizeof(packed_type);
        lua_pushnil(L);
        break;
      case TV_TRUE:
      case TV_FALSE:
        ptr += sizeof(packed_type);
        lua_pushboolean(L, type == TV_TRUE);
        break;
      case TV_NUMBER: {
        lua_Number n;
        ptr += sizeof(packed_type);
        memcpy(&n, ptr, sizeof n);
        ptr += sizeof(lua_Number);
        lua_pushnumber(L, n);
        break;
      }
      case TV_STRING: {
        size_t s;
        ptr += sizeof(packed_type);
        memcpy(&s, ptr, sizeof s);
        ptr += sizeof s;
        lua_pushlstring(L, ptr, s);
        ptr += s;
        break;
      }
      case TV_OBJECT: {
        lua_apr_objtype *objtype;
        lua_apr_refobj *object;
        ptr += sizeof(packed_type);
        memcpy(&objtype, ptr, sizeof objtype);
        ptr += sizeof objtype;
        memcpy(&object, ptr, sizeof object);
        ptr += sizeof object;
        create_reference(L, objtype, object);
        break;
      }
      default:
        LUA_APR_DBG("#%i: Failed to unpack value! (type %i)", i + 1, (unsigned int) type);
        return i;
    } /* switch */
  } /* for */

  return i;
}
