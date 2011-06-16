/* XML parsing module for the Lua/APR binding.
 *
 * Authors:
 *  - zhiguo zhao <zhaozg@gmail.com>
 *  - Peter Odding <peter@peterodding.com>
 * Last Change: June 16, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * This module enables parsing of [XML] [xml] documents. Unlike [LuaExpat]
 * [lxp] the parsers returned by `apr.xml()` don't use callbacks, instead they
 * parse XML into a [document object model] [dom] which is then exposed to Lua.
 * Because of this you can't use `apr.xml()` for incremental parsing. To parse
 * an XML document you follow these steps:
 *
 * 1. Create an XML parser object by calling `apr.xml()`
 * 2. Parse the document by calling `xml_parser:feed()` repeatedly until the
 *    full document has been parsed
 * 3. Signal to the parser that the full document has been parsed by calling
 *    `xml_parser:done()`
 * 4. Get the parse information by calling `xml_parser:getinfo()`
 *
 * Right now the only way to get the parse information is by calling
 * `xml_parser:getinfo()` which converts the information to a Lua table
 * following the [Lua object model] [lom] defined by [LuaExpat] [lxp]. The Lua
 * object model is a mapping of XML to Lua tables that's not 100% complete
 * (e.g. it doesn't include namespaces) but makes it a lot easier to deal with
 * XML in Lua.
 *
 * In the future this module might expose the full XML parse tree to Lua as
 * userdata objects, so that Lua has access to all parse information. This
 * would also make it possible to expose the [apr\_xml\_to\_text()]
 * [xml_to_text] function.
 *
 * [xml]: http://en.wikipedia.org/wiki/XML
 * [lxp]: http://www.keplerproject.org/luaexpat/
 * [lom]: http://www.keplerproject.org/luaexpat/lom.html
 * [dom]: http://en.wikipedia.org/wiki/XML#Document_Object_Model_.28DOM.29
 * [xml_to_text]: http://apr.apache.org/docs/apr/trunk/group___a_p_r___util___x_m_l.html#g4485edce130dc1e9a3da3a633a75ffb3
 */

#include "lua_apr.h"
#include <apr_xml.h>
#include <apr_xlate.h>

typedef struct {
  lua_apr_refobj header;
  apr_pool_t *pool;
  apr_xml_parser *parser;
  apr_xml_doc *doc;
} lua_apr_xml_object;

typedef enum {
  CHECK_NONE,
  CHECK_PARSER,
  CHECK_DOCUMENT,
} object_state;

/* Internal functions. {{{1 */

static lua_apr_xml_object *check_xml_parser(lua_State *L, int idx, object_state state)
{
  lua_apr_xml_object *object;

  object = check_object(L, idx, &lua_apr_xml_type);
  if (state == CHECK_PARSER && object->parser == NULL)
    luaL_error(L, "attempt to use a closed XML parser");
  if (state == CHECK_DOCUMENT && object->doc == NULL)
    luaL_error(L, "no parse information available");

  return object;
}

static int push_xml_status(lua_State *L, lua_apr_xml_object *object, apr_status_t status)
{
  if (status == APR_SUCCESS) {
    lua_pushboolean(L, 1);
    return 1;
  } else {
    char message[LUA_APR_MSGSIZE];
    apr_xml_parser_geterror(object->parser, message, sizeof(message));
    lua_pushnil(L);
    lua_pushstring(L, message);
    return 2;
  }
}

static void reverse_table(lua_State *L, int idx, int len)
{
  int i, j;
  for (i = 1, j = len; i < j; i++, j--) {
    lua_rawgeti(L, idx, i);
    lua_rawgeti(L, idx, j);
    lua_rawseti(L, idx, i);
    lua_rawseti(L, idx, j);
  }
}

static void dump_xml(lua_State *L, apr_xml_elem *elem)
{
  apr_xml_attr *attr;
  apr_xml_elem *child;
  apr_text *text;
  int i;

  luaL_checktype(L, -1, LUA_TTABLE);
  if (elem->name != NULL) {
    lua_pushstring(L, "tag");
    lua_pushstring(L, elem->name);
    lua_rawset(L, -3);
  }
  if (elem->attr != NULL) {
    lua_newtable(L);
    i = 1;
    attr = elem->attr;
    while (attr != NULL) {
      lua_pushstring(L, attr->name);
      lua_rawseti(L, -2, i);
      lua_pushstring(L, attr->name);
      lua_pushstring(L, attr->value);
      lua_rawset(L, -3);
      attr = attr->next;
      i++;
    }
    /* XXX Reverse the order of the attributes in the array part of the table.
     * The apr_xml.h header doesn't document that attributes are reversed, in
     * fact the apr_xml_elem.attr field is documented as the "first attribute",
     * but in the definition of start_handler() in the apr_xml.c implementation
     * it _is_ mentioned that attributes end up in reverse order. */
    reverse_table(L, lua_gettop(L), i - 1);
    lua_setfield(L, -2, "attr");
  }
  i = 1;
  if (elem->first_child != NULL) {
    child = elem->first_child;
    while (child != NULL) {
      lua_newtable(L);
      dump_xml(L, child);
      lua_rawseti(L, -2, i);
      child = child->next;
      i++;
    }
  } else {
    text = elem->first_cdata.first;
    while (text != NULL) {
      lua_pushstring(L, text->text);
      lua_rawseti(L, -2, i);
      text = text->next;
      i++;
    }
  }
}

static void xml_close_real(lua_apr_xml_object *object)
{
  if (object->parser != NULL) {
    object->parser = NULL;
    apr_pool_destroy(object->pool);
    object->pool = NULL;
  }
}

/* apr.xml([filename]) -> xml_parser {{{1
 *
 * Create an XML parser. If the optional string @filename is given, the file
 * pointed to by @filename will be parsed. On success the parser object is
 * returned, otherwise a nil followed by an error message is returned.
 */

int lua_apr_xml(lua_State *L)
{
  lua_apr_xml_object *object;
  apr_status_t status;
  const char *filename;

  filename = luaL_optstring(L, 1, NULL);
  object = new_object(L, &lua_apr_xml_type);
  if (object == NULL)
    return push_error_memory(L);
  status = apr_pool_create(&object->pool, NULL);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);

  if (filename == NULL) {
    object->parser = apr_xml_parser_create(object->pool);
    if (object->parser == NULL)
      return push_error_memory(L);
  } else {
    apr_finfo_t info;
    apr_file_t *xmlfd;
    status = apr_file_open(&xmlfd, filename, APR_FOPEN_READ, 0, object->pool);
    if (status != APR_SUCCESS)
      return push_status(L, status);
    status = apr_file_info_get(&info, APR_FINFO_SIZE, xmlfd);
    if (status != APR_SUCCESS) {
      apr_file_close(xmlfd);
      return push_status(L, status);
    }
    status = apr_xml_parse_file(object->pool, &object->parser, &object->doc, xmlfd, (apr_size_t)info.size);
    apr_file_close(xmlfd);
    if (status != APR_SUCCESS)
      return push_xml_status(L, object, status);
  }

  return 1;
}

/* xml_parser:feed(input) -> status {{{1
 *
 * Feed the string @input into the XML parser. On success true is returned,
 * otherwise a nil followed by an error message is returned.
 */

static int xml_feed(lua_State *L)
{
  lua_apr_xml_object *object;
  apr_status_t status;
  apr_size_t size;
  const char *data;

  object = check_xml_parser(L, 1, CHECK_PARSER);
  data = luaL_checklstring(L, 2, &size);
  status = apr_xml_parser_feed(object->parser, data, size);

  return push_xml_status(L, object, status);
}

/* xml_parser:done() -> status {{{1
 *
 * Terminate the parsing and save the resulting parse information. On success
 * true is returned, otherwise a nil followed by an error message is returned.
 */

static int xml_done(lua_State *L)
{
  lua_apr_xml_object *object;
  apr_status_t status;

  object = check_xml_parser(L, 1, CHECK_PARSER);
  status = apr_xml_parser_done(object->parser, &object->doc);

  return push_xml_status(L, object, status);
}

/* xml_parser:geterror() -> message {{{1
 *
 * Fetch additional error information from the parser after one of its methods
 * has failed.
 */

static int xml_geterror(lua_State *L)
{
  lua_apr_xml_object *object;
  char message[LUA_APR_MSGSIZE];

  object = check_xml_parser(L, 1, CHECK_PARSER);
  apr_xml_parser_geterror(object->parser, message, sizeof(message));
  lua_pushstring(L, message);

  return 1;
}

/* xml_parser:getinfo() -> table {{{1
 *
 * Convert the parse information to a Lua table following the
 * [Lua Object Model] [lom] defined by [LuaExpat] [lxp].
 */

static int xml_getinfo(lua_State *L)
{
  lua_apr_xml_object *object;

  object = check_xml_parser(L, 1, CHECK_DOCUMENT);
  lua_newtable(L);
  dump_xml(L, object->doc->root);

  return 1;
}

/* xml_parser:close() -> status {{{1
 *
 * Close the XML parser and destroy any parse information. This will be done
 * automatically when the @xml_parser object is garbage collected which means
 * you don't need to call this unless you want to reclaim memory as soon as
 * possible (e.g. because you just parsed a large XML document).
 */

static int xml_close(lua_State *L)
{
  lua_apr_xml_object *object;

  object = check_xml_parser(L, 1, CHECK_NONE);
  xml_close_real(object);
  lua_pushboolean(L, 1);

  return 1;
}

/* tostring(xml_parser) -> string {{{1 */

static int xml_tostring(lua_State *L)
{
  lua_apr_xml_object *object;

  object = check_xml_parser(L, 1, CHECK_NONE);
  if (object->parser != NULL)
    lua_pushfstring(L, "%s (%p)", lua_apr_xml_type.friendlyname, object);
  else
    lua_pushfstring(L, "%s (closed)", lua_apr_xml_type.friendlyname);

  return 1;
}

/* xml_parser:__gc() {{{1 */

static int xml_gc(lua_State *L)
{
  lua_apr_xml_object *object = check_xml_parser(L, 1, CHECK_NONE);
  if (object_collectable((lua_apr_refobj*)object))
    xml_close_real(object);
  release_object((lua_apr_refobj*)object);
  return 0;
}

/* }}}1 */

static luaL_reg xml_methods[] = {
  { "feed", xml_feed },
  { "done", xml_done },
  { "getinfo", xml_getinfo },
  { "geterror", xml_geterror },
  { "close", xml_close },
  { NULL, NULL }
};

static luaL_reg xml_metamethods[] = {
  { "__tostring", xml_tostring },
  { "__eq", objects_equal },
  { "__gc", xml_gc },
  { NULL, NULL }
};

lua_apr_objtype lua_apr_xml_type = {
  "lua_apr_xml_object*",      /* metatable name in registry */
  "xml parser",               /* friendly object name */
  sizeof(lua_apr_xml_object), /* structure size */
  xml_methods,                /* methods table */
  xml_metamethods             /* metamethods table */
};
