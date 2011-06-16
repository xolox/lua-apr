/* Miscellaneous functions module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: June 16, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 */

#include "lua_apr.h"
#include <apr_portable.h>
#include <ctype.h>

/* Used to make sure that APR is only initialized once. */
static int apr_was_initialized = 0;

/* List of all userdata types exposed to Lua by the binding. */
lua_apr_objtype *lua_apr_types[] = {
  &lua_apr_file_type,
  &lua_apr_dir_type,
  &lua_apr_socket_type,
# if APR_HAS_THREADS
  &lua_apr_thread_type,
  &lua_apr_queue_type,
# endif
  &lua_apr_proc_type,
  &lua_apr_dbm_type,
  &lua_apr_dbd_type,
  &lua_apr_dbr_type,
  &lua_apr_dbp_type,
  &lua_apr_memcache_type,
  &lua_apr_memcache_server_type,
  &lua_apr_md5_type,
  &lua_apr_sha1_type,
  &lua_apr_xml_type,
  NULL
};

/* luaopen_apr_core() initializes the binding and library. {{{1 */

LUA_APR_EXPORT int luaopen_apr_core(lua_State *L)
{
  apr_status_t status;

  /* Table of library functions. */
  luaL_Reg functions[] = {

    /* lua_apr.c -- miscellaneous functions. */
    { "platform_get", lua_apr_platform_get },
    { "version_get", lua_apr_version_get },
    { "os_default_encoding", lua_apr_os_default_encoding },
    { "os_locale_encoding", lua_apr_os_locale_encoding },
    { "type", lua_apr_type },

    /* base64.c -- base64 encoding/decoding. */
    { "base64_encode", lua_apr_base64_encode },
    { "base64_decode", lua_apr_base64_decode },

    /* crypt.c -- cryptographic functions. */
    { "md5_init", lua_apr_md5_init },
    { "md5_encode", lua_apr_md5_encode },
    { "password_get", lua_apr_password_get },
    { "password_validate", lua_apr_password_validate },
    { "sha1_init", lua_apr_sha1_init },

    /* date.c -- date parsing. */
    { "date_parse_http", lua_apr_date_parse_http },
    { "date_parse_rfc", lua_apr_date_parse_rfc },

    /* dbd.c -- database module. */
    { "dbd", lua_apr_dbd },

    /* dbm.c -- dbm routines. */
    { "dbm_open", lua_apr_dbm_open },
    { "dbm_getnames", lua_apr_dbm_getnames },

    /* env.c -- environment variable handling. */
    { "env_get", lua_apr_env_get },
    { "env_set", lua_apr_env_set },
    { "env_delete", lua_apr_env_delete },

    /* filepath.c -- filepath manipulation. */
    { "filepath_root", lua_apr_filepath_root },
    { "filepath_parent", lua_apr_filepath_parent },
    { "filepath_name", lua_apr_filepath_name },
    { "filepath_merge", lua_apr_filepath_merge },
    { "filepath_list_split", lua_apr_filepath_list_split },
    { "filepath_list_merge", lua_apr_filepath_list_merge },
    { "filepath_get", lua_apr_filepath_get },
    { "filepath_set", lua_apr_filepath_set },

    /* fnmatch.c -- filename matching. */
    { "fnmatch", lua_apr_fnmatch },
    { "fnmatch_test", lua_apr_fnmatch_test },

    /* getopt.c -- command argument parsing. */
    { "getopt", lua_apr_getopt },

#   ifndef LUA_APR_DISABLE_APREQ
    /* http.c -- HTTP request parsing. */
    { "parse_headers", lua_apr_parse_headers },
    { "parse_multipart", lua_apr_parse_multipart },
    { "parse_cookie_header", lua_apr_parse_cookie_header },
    { "parse_query_string", lua_apr_parse_query_string },
    { "header_attribute", lua_apr_header_attribute },
    { "uri_encode", lua_apr_uri_encode },
    { "uri_decode", lua_apr_uri_decode },
#   endif

    /* io_dir.c -- directory manipulation. */
    { "temp_dir_get", lua_apr_temp_dir_get },
    { "dir_make", lua_apr_dir_make },
    { "dir_make_recursive", lua_apr_dir_make_recursive },
    { "dir_remove", lua_apr_dir_remove },
    { "dir_remove_recursive", lua_apr_dir_remove_recursive },
    { "dir_open", lua_apr_dir_open },

    /* io_file.c -- file i/o handling. */
#   if APR_MAJOR_VERSION > 1 || (APR_MAJOR_VERSION == 1 && APR_MINOR_VERSION >= 4)
    { "file_link", lua_apr_file_link },
#   endif
    { "file_copy", lua_apr_file_copy },
    { "file_append", lua_apr_file_append },
    { "file_rename", lua_apr_file_rename },
    { "file_remove", lua_apr_file_remove },
    { "file_mtime_set", lua_apr_file_mtime_set },
    { "file_attrs_set", lua_apr_file_attrs_set },
    { "file_perms_set", lua_apr_file_perms_set },
    { "stat", lua_apr_stat },
    { "file_open", lua_apr_file_open },

    /* io_net.c -- network i/o handling. */
    { "socket_create", lua_apr_socket_create },
    { "hostname_get", lua_apr_hostname_get },
    { "host_to_addr", lua_apr_host_to_addr },
    { "addr_to_host", lua_apr_addr_to_host },

    /* io_pipe.c -- pipe i/o handling. */
    { "pipe_open_stdin", lua_apr_pipe_open_stdin },
    { "pipe_open_stdout", lua_apr_pipe_open_stdout },
    { "pipe_open_stderr", lua_apr_pipe_open_stderr },
    { "namedpipe_create", lua_apr_namedpipe_create },
    { "pipe_create", lua_apr_pipe_create },

    /* proc -- process handling. */
    { "proc_create", lua_apr_proc_create },
    { "proc_detach", lua_apr_proc_detach },
#   if APR_HAS_FORK
    { "proc_fork", lua_apr_proc_fork },
#   endif

    /* shm.c -- shared memory. */
    { "shm_create", lua_apr_shm_create },
    { "shm_attach", lua_apr_shm_attach },
    { "shm_remove", lua_apr_shm_remove },

    /* str.c -- string handling. */
    { "strnatcmp", lua_apr_strnatcmp },
    { "strnatcasecmp", lua_apr_strnatcasecmp },
    { "strfsize", lua_apr_strfsize },
    { "tokenize_to_argv", lua_apr_tokenize_to_argv },

#   if APR_HAS_THREADS

    /* thread.c -- multi threading. */
    { "thread_create", lua_apr_thread_create },
    { "thread_yield", lua_apr_thread_yield },

    /* thread_queue.c -- thread queues. */
    { "thread_queue", lua_apr_thread_queue },

#   endif

    /* time.c -- time management */
    { "sleep", lua_apr_sleep },
    { "time_now", lua_apr_time_now },
    { "time_explode", lua_apr_time_explode },
    { "time_implode", lua_apr_time_implode },
    { "time_format", lua_apr_time_format },

    /* uri.c -- URI parsing/unparsing. */
    { "uri_parse", lua_apr_uri_parse },
    { "uri_unparse", lua_apr_uri_unparse },
    { "uri_port_of_scheme", lua_apr_uri_port_of_scheme },

    /* user.c -- user/group identification. */
    { "user_get", lua_apr_user_get },
    { "user_homepath_get", lua_apr_user_homepath_get },

    /* uuid.c -- UUID generation. */
    { "uuid_get", lua_apr_uuid_get },
    { "uuid_format", lua_apr_uuid_format },
    { "uuid_parse", lua_apr_uuid_parse },

    /* xlate.c -- character encoding translation. */
    { "xlate", lua_apr_xlate },

    /* xml.c -- XML parsing. */
    { "xml", lua_apr_xml },

    /* memcache.c -- memcached client. */
    { "memcache", lua_apr_memcache },

    { NULL, NULL }
  };

  /* Initialize the library (only once per process). */
  if (!apr_was_initialized) {
    if ((status = apr_initialize()) != APR_SUCCESS)
      raise_error_status(L, status);
    if (atexit(apr_terminate) != 0)
      raise_error_message(L, "Lua/APR: Failed to register apr_terminate()");
    apr_was_initialized = 1;
  }

  /* Create the `scratch' memory pool for global APR functions (as opposed to
   * object methods) and install a __gc metamethod to detect when the Lua state
   * is exited. */
  to_pool(L);

  /* Create the table of global functions. */
  lua_createtable(L, 0, count(functions));
  luaL_register(L, NULL, functions);

  /* Let callers of process:user_set() know whether it requires a password. */
  lua_pushboolean(L, APR_PROCATTR_USER_SET_REQUIRES_PASSWORD);
  lua_setfield(L, -2, "user_set_requires_password");

  /* Let callers of apr.socket_create() know whether it supports IPv6. */
  lua_pushboolean(L, APR_HAVE_IPV6);
  lua_setfield(L, -2, "socket_supports_ipv6");

  return 1;
}

/* apr.platform_get() -> name {{{1
 *
 * Get the name of the platform for which the Lua/APR binding was compiled.
 * Returns one of the following strings:
 *
 *  - `'UNIX'`
 *  - `'WIN32'`
 *  - `'NETWARE'`
 *  - `'OS2'`
 *
 * Please note that the labels returned by `apr.platform_get()` don't imply
 * that these platforms are fully supported; the author of the Lua/APR binding
 * doesn't have NETWARE and OS2 environments available for testing.
 */

int lua_apr_platform_get(lua_State *L)
{
# if defined(WIN32)
  lua_pushstring(L, "WIN32");
# elif defined(NETWARE)
  lua_pushstring(L, "NETWARE");
# elif defined(OS2)
  lua_pushstring(L, "OS2");
# else
  lua_pushstring(L, "UNIX");
# endif
  return 1;
}

/* apr.version_get() -> apr_version [, apu_version] {{{1
 *
 * Get the version number of the Apache Portable Runtime as a string. The
 * string contains three numbers separated by dots. These numbers have the
 * following meaning:
 *
 *  - The 1st number is used for major [API] [api] changes that can cause
 *    compatibility problems between the Lua/APR binding and APR library
 *  - The 2nd number is used for minor API changes that shouldn't impact
 *    existing functionality in the Lua/APR binding
 *  - The 3rd number is used exclusively for bug fixes
 *
 * The second return value, the version number of the APR utility library, is
 * only available when Lua/APR is compiled against APR 1.x because in APR 2.x
 * the utility library has been absorbed back into the APR library; there is no
 * longer a distinction between the APR core and APR utility libraries.
 *
 * This function can be useful when you want to know whether a certain bug fix
 * has been applied to APR and/or APR-util or if you want to report a bug in
 * APR, APR-util or the Lua/APR binding.
 *
 * If you're looking for the version of the Lua/APR binding you can use the
 * `apr._VERSION` string, but note that Lua/APR currently does not adhere to
 * the above versioning rules.
 *
 * [api]: http://en.wikipedia.org/wiki/Application_programming_interface
 */

int lua_apr_version_get(lua_State *L)
{
  lua_pushstring(L, apr_version_string());
# if APR_MAJOR_VERSION < 2
  lua_pushstring(L, apu_version_string());
  return 2;
# else
  return 1;
# endif
}

/* apr.os_default_encoding() -> name {{{1
 *
 * Get the name of the system default character set as a string.
 */

int lua_apr_os_default_encoding(lua_State *L)
{
  lua_pushstring(L, apr_os_default_encoding(to_pool(L)));
  return 1;
}

/* apr.os_locale_encoding() -> name {{{1
 *
 * Get the name of the current locale character set as a string. If the current
 * locale's data cannot be retrieved on this system, the name of the system
 * default character set is returned instead.
 */

int lua_apr_os_locale_encoding(lua_State *L)
{
  lua_pushstring(L, apr_os_locale_encoding(to_pool(L)));
  return 1;
}

/* apr.type(object) -> name {{{1
 *
 * Return the type of a userdata object created by the Lua/APR binding. If
 * @object is of a known type one of the following strings will be returned,
 * otherwise nothing is returned:
 *
 *  - `'file'`
 *  - `'directory'`
 *  - `'socket'`
 *  - `'thread'`
 *  - `'process'`
 *  - `'dbm'`
 *  - `'database driver'`
 *  - `'prepared statement'`
 *  - `'result set'`
 *  - `'memcache client'`
 *  - `'memcache server'`
 *  - `'md5 context'`
 *  - `'sha1 context'`
 *  - `'xml parser'`
 */

int lua_apr_type(lua_State *L)
{
  int i;

  lua_settop(L, 1);
  luaL_checktype(L, 1, LUA_TUSERDATA);
  lua_getmetatable(L, 1);

  for (i = 0; lua_apr_types[i] != NULL; i++) {
    if (object_has_type(L, 1, lua_apr_types[i], 0)) {
      lua_pushstring(L, lua_apr_types[i]->friendlyname);
      return 1;
    }
  }

  return 0;
}

/* status_to_message() converts APR status codes to error messages. {{{1 */

int status_to_message(lua_State *L, apr_status_t status)
{
  int length;
  char message[LUA_APR_MSGSIZE];

  apr_strerror(status, message, count(message));
  length = strlen(message);
  while (length > 0 && isspace(message[length - 1]))
    length--;
  lua_pushlstring(L, message, length);
  return 1;
}

/* push_status() returns true for APR_SUCCESS or the result of status_to_message(). {{{1 */

int push_status(lua_State *L, apr_status_t status)
{
  if (status == APR_SUCCESS) {
    lua_pushboolean(L, 1);
    return 1;
  } else {
    return push_error_status(L, status);
  }
}

/* push_error_status() converts APR status codes to (nil, message, code). {{{1 */

int push_error_status(lua_State *L, apr_status_t status)
{
  lua_pushnil(L);
  status_to_message(L, status);
  status_to_name(L, status);
  return 3;
}

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
