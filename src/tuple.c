/* Inter thread serialization module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: June 16, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
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
        return sizeof(packed_type) + sizeof(lua_apr_objtype**) + sizeof(void**);
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
      *(packed_type*)value = TV_NUMBER;
      *(lua_Number*)(p + sizeof(packed_type)) = lua_tonumber(L, idx);
      return 1;
    }
    case LUA_TSTRING: {
      size_t size;
      const char *data = lua_tolstring(L, idx, &size);
      *(packed_type*)value = TV_STRING;
      *(size_t*)(p += sizeof(packed_type)) = size;
      memcpy(p + sizeof(size_t), data, size);
      return 1;
    }
    case LUA_TUSERDATA: {
      /* TODO Avoid this second loop using lua_apr_objtype[] array on stack. */
      int i;
      for (i = 0; lua_apr_types[i] != NULL; i++) {
        lua_apr_objtype *T = lua_apr_types[i];
        if (object_has_type(L, idx, T, 1)) {
          lua_apr_refobj *refobj = lua_touserdata(L, idx);
          *(packed_type*)value = TV_OBJECT;
          *(lua_apr_objtype**)(p += sizeof(packed_type)) = T;
          *(void**)(p += sizeof(lua_apr_objtype**)) = prepare_reference(T, refobj);
          /* XXX Increase the reference count assuming the tuple will be
           * unpacked eventually (barring exceptional errors?!) */
          object_incref(refobj);
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

  *(size_t*)tuple = 0; /* start with tuple of zero values */
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
        lua_pushnil(L);
        ptr += sizeof(packed_type);
        break;
      case TV_TRUE:
      case TV_FALSE:
        lua_pushboolean(L, type == TV_TRUE);
        ptr += sizeof(packed_type);
        break;
      case TV_NUMBER: {
        lua_Number number = *(lua_Number*)(ptr += sizeof(packed_type));
        lua_pushnumber(L, number);
        ptr += sizeof(lua_Number);
        break;
      }
      case TV_STRING: {
        size_t size = *(size_t*)(ptr += sizeof(packed_type));
        const char *data = (ptr += sizeof(size_t));
        lua_pushlstring(L, data, size);
        ptr += size;
        break;
      }
      case TV_OBJECT: {
        lua_apr_objtype *objtype = *(lua_apr_objtype**)(ptr += sizeof(packed_type));
        lua_apr_refobj *object = *(void**)(ptr += sizeof(lua_apr_objtype**));
        lua_apr_refobj *reference = lua_newuserdata(L, sizeof(lua_apr_refobj));
        if (reference == NULL) {
          /* TODO Raise proper error message?
           * TODO Cleanup references to already packed objects?! */
          LUA_APR_DBG("#%i: Failed to unpack userdata!", i + 1);
          return i;
        }
        reference->reference = object;
        reference->unmanaged = 0;
        init_object(L, objtype);
        ptr += sizeof(void**);
        break;
      }
      default:
        LUA_APR_DBG("#%i: Failed to unpack value! (type %i)", i + 1, (unsigned int) type);
        return i;
    } /* switch */
  } /* for */

  return i;
}
