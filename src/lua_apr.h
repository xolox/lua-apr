/* Header file for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: June 16, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 */

#ifndef LUA_APR_H
#define LUA_APR_H

/* Global headers. {{{1 */
#include <assert.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <apr.h>
#include <apu.h>
#include <apr_version.h>
#include <apu_version.h>
#include <apr_general.h>
#include <apr_file_info.h>
#include <apr_file_io.h>
#include <apr_thread_proc.h>
#include <apr_queue.h>

/* Macro definitions. {{{1 */

/* Enable redefining exporting of loader function, with sane defaults. */
#ifndef LUA_APR_EXPORT
# ifdef WIN32
#  define LUA_APR_EXPORT __declspec(dllexport)
# else
#  define LUA_APR_EXPORT extern
# endif
#endif

#define LUA_APR_POOL_KEY "Lua/APR memory pool"
#define LUA_APR_POOL_MT "Lua/APR memory pool metamethods"

/* FIXME Pushing onto the stack might not work in this scenario? */
#define error_message_memory "memory allocation error"

/* Size of error message buffers on stack. */
#define LUA_APR_MSGSIZE 512

/* The initial size of I/O buffers. */
#define LUA_APR_BUFSIZE 1024

/* Slack space at the end of I/O buffers. */
#define LUA_APR_BUFSLACK 10

#define count(array) \
  (sizeof((array)) / sizeof((array)[0]))

#define filename_symbolic(S) \
  ((S)[0] == '.' && ( \
    (S)[1] == '\0' || ( \
      (S)[1] == '.' && \
      (S)[2] == '\0')))

#define raise_error_message(L, message) \
  (lua_pushstring((L), (message)), lua_error((L)))

#define raise_error_status(L, status) \
  (status_to_message((L), (status)), lua_error((L)))

#define raise_error_memory(L) \
  raise_error_message((L), (error_message_memory))

#define push_error_message(L, message) \
  (lua_pushnil((L)), lua_pushstring((L), (message)), 2)

#define push_error_memory(L) \
  push_error_message((L), (error_message_memory))

#define time_get(L, idx) \
  ((apr_time_t) (lua_tonumber(L, idx) * APR_USEC_PER_SEC))

#define time_put(L, time) \
  lua_pushnumber(L, (lua_Number)time / APR_USEC_PER_SEC)

#define push_string_or_true(L, s) \
  (s != NULL && s[0] != '\0' ? lua_pushstring(L, s) : lua_pushboolean(L, 1))

/* Debugging aids. {{{1 */

#include <stdio.h>

/* Capture debug mode in a compile time conditional. */
#ifdef DEBUG
#define DEBUG_TEST 1
#else
#define DEBUG_TEST 0
#endif

/* Enable printing messages to stderr in debug mode, but always compile
 * the code so that it's syntax checked even when debug mode is off. */
#define LUA_APR_DBG(MSG, ...) do {                  \
  if (DEBUG_TEST) {                                 \
    fprintf(stderr, " *** " MSG "\n", __VA_ARGS__); \
    fflush(stderr);                                 \
  }                                                 \
} while (0)

/* Since I don't use the MSVC++ IDE I can't set breakpoints without this :-) */
#define LUA_APR_BRK() do { \
  if (DEBUG_TEST)          \
    *((int*)0) = 1;        \
} while (0)

/* Dump the state of the Lua stack. */
#define LUA_APR_DUMP_STACK(L, LABEL) do { \
  int i, top = lua_gettop(L); \
  LUA_APR_DBG("%s: %i", LABEL, top); \
  for (i = 1; i <= top; i++) { \
    switch (lua_type(L, i)) { \
      case LUA_TNIL: LUA_APR_DBG(" %i: nil", i); break; \
      case LUA_TBOOLEAN: LUA_APR_DBG(" %i: boolean %s", i, lua_toboolean(L, i) ? "true" : "false"); \
      case LUA_TNUMBER: LUA_APR_DBG(" %i: number %f", i, lua_tonumber(L, i)); break; \
      case LUA_TSTRING: LUA_APR_DBG(" %i: string \"%.*s\"", i, lua_objlen(L, i), lua_tostring(L, i)); break; \
      default: LUA_APR_DBG(" %i: %s", i, luaL_typename(L, i)); break; \
    } \
  } \
} while (0)

/* Type definitions. {{{1 */

/* Reference counted Lua objects. */
typedef struct lua_apr_refobj lua_apr_refobj;
struct lua_apr_refobj {
  lua_apr_refobj *reference;
  int refcount, unmanaged;
};

/* Reference counted APR memory pool. */
typedef struct {
  apr_pool_t *ptr;
  int refs;
} lua_apr_pool;

/* Context structure for stat() calls. */
typedef struct {
  apr_int32_t wanted;
  apr_finfo_t info;
  apr_int32_t fields[15];
  int firstarg, lastarg, count;
} lua_apr_stat_context;

/* The calling convention for APR functions and callbacks. On Windows this
 * means __stdcall and omitting it can cause nasty stack corruption! */
#define lua_apr_cc APR_THREAD_FUNC

/* Type definitions used to call APR functions through function pointers. */
typedef apr_status_t (lua_apr_cc *lua_apr_buf_rf)(void*, char*, apr_size_t*);
typedef apr_status_t (lua_apr_cc *lua_apr_buf_wf)(void*, const char*, apr_size_t*);
typedef apr_status_t (lua_apr_cc *lua_apr_buf_ff)(void*);
typedef apr_status_t (lua_apr_cc *lua_apr_openpipe_f)(apr_file_t**, apr_pool_t*);
typedef apr_status_t (lua_apr_cc *lua_apr_setpipe_f)(apr_procattr_t*, apr_file_t*, apr_file_t*);

/* Structures for (buffered) I/O streams. */

typedef struct {
  int unmanaged;
  size_t index, limit, size;
  char *data;
} lua_apr_buffer;

typedef struct {
  int text_mode;
  void *object;
  lua_apr_buf_rf read;
  lua_apr_buffer buffer;
} lua_apr_readbuf;

typedef struct {
  int text_mode;
  void *object;
  lua_apr_buf_wf write;
  lua_apr_buf_ff flush;
  lua_apr_buffer buffer;
} lua_apr_writebuf;

/* Structure for file objects. */
typedef struct {
  lua_apr_refobj header;
  lua_apr_readbuf input;
  lua_apr_writebuf output;
  apr_file_t *handle;
  lua_apr_pool *pool;
  const char *path;
} lua_apr_file;

/* Structure used to define Lua userdata types created by Lua/APR. */
typedef struct {
  const char *typename, *friendlyname;
  const size_t objsize;
  luaL_Reg *methods;
  luaL_Reg *metamethods;
} lua_apr_objtype;

/* External type definitions. */
extern lua_apr_objtype *lua_apr_types[];
extern lua_apr_objtype lua_apr_file_type;
extern lua_apr_objtype lua_apr_dir_type;
extern lua_apr_objtype lua_apr_socket_type;
extern lua_apr_objtype lua_apr_thread_type;
extern lua_apr_objtype lua_apr_queue_type;
extern lua_apr_objtype lua_apr_proc_type;
extern lua_apr_objtype lua_apr_shm_type;
extern lua_apr_objtype lua_apr_dbm_type;
extern lua_apr_objtype lua_apr_dbd_type;
extern lua_apr_objtype lua_apr_dbr_type;
extern lua_apr_objtype lua_apr_dbp_type;
extern lua_apr_objtype lua_apr_md5_type;
extern lua_apr_objtype lua_apr_sha1_type;
extern lua_apr_objtype lua_apr_xml_type;
extern lua_apr_objtype lua_apr_memcache_type;
extern lua_apr_objtype lua_apr_memcache_server_type;

/* Prototypes. {{{1 */

/* lua_apr.c */
int lua_apr_platform_get(lua_State*);
int lua_apr_version_get(lua_State*);
int lua_apr_os_default_encoding(lua_State*);
int lua_apr_os_locale_encoding(lua_State*);
int lua_apr_type(lua_State*);
int status_to_message(lua_State*, apr_status_t);
int push_status(lua_State*, apr_status_t);
int push_error_status(lua_State*, apr_status_t);

/* base64.c */
int lua_apr_base64_encode(lua_State*);
int lua_apr_base64_decode(lua_State*);

/* buffer.c */
void init_buffers(lua_State*, lua_apr_readbuf*, lua_apr_writebuf*, void*, int,
                  lua_apr_buf_rf, lua_apr_buf_wf, lua_apr_buf_ff);
void init_unmanaged_buffers(lua_State*, lua_apr_readbuf*, lua_apr_writebuf*, char*, size_t);
int read_lines(lua_State*, lua_apr_readbuf*);
int read_buffer(lua_State*, lua_apr_readbuf*);
int write_buffer(lua_State*, lua_apr_writebuf*);
apr_status_t flush_buffer(lua_State*, lua_apr_writebuf*, int);
void free_buffer(lua_State*, lua_apr_buffer*);

/* crypt.c */
int lua_apr_md5_init(lua_State*);
int lua_apr_md5_encode(lua_State*);
int lua_apr_sha1_init(lua_State*);
int lua_apr_password_validate(lua_State*);
int lua_apr_password_get(lua_State*);

/* date.c */
int lua_apr_date_parse_http(lua_State*);
int lua_apr_date_parse_rfc(lua_State*);

/* dbd.c */
int lua_apr_dbd(lua_State*);

/* dbm.c */
int lua_apr_dbm_open(lua_State*);
int lua_apr_dbm_getnames(lua_State*);

/* env.c */
int lua_apr_env_get(lua_State*);
int lua_apr_env_set(lua_State*);
int lua_apr_env_delete(lua_State*);

/* errno.c */
void status_to_name(lua_State*, apr_status_t);

/* filepath.c */
int lua_apr_filepath_root(lua_State*);
int lua_apr_filepath_parent(lua_State*);
int lua_apr_filepath_name(lua_State*);
int lua_apr_filepath_merge(lua_State*);
int lua_apr_filepath_list_split(lua_State*);
int lua_apr_filepath_list_merge(lua_State*);
int lua_apr_filepath_get(lua_State*);
int lua_apr_filepath_set(lua_State*);

/* fnmatch.c */
int lua_apr_fnmatch(lua_State*);
int lua_apr_fnmatch_test(lua_State*);

/* getopt.c */
int lua_apr_getopt(lua_State*);

/* http.c */
int lua_apr_parse_headers(lua_State*);
int lua_apr_parse_multipart(lua_State*);
int lua_apr_parse_cookie_header(lua_State*);
int lua_apr_parse_query_string(lua_State*);
int lua_apr_header_attribute(lua_State*);
int lua_apr_uri_encode(lua_State*);
int lua_apr_uri_decode(lua_State*);

/* io_dir.c */
int lua_apr_temp_dir_get(lua_State*);
int lua_apr_dir_make(lua_State*);
int lua_apr_dir_make_recursive(lua_State*);
int lua_apr_dir_remove(lua_State*);
int lua_apr_dir_remove_recursive(lua_State*);
int lua_apr_dir_open(lua_State*);

/* io_file.c */
int lua_apr_file_link(lua_State*);
int lua_apr_file_copy(lua_State*);
int lua_apr_file_append(lua_State*);
int lua_apr_file_rename(lua_State*);
int lua_apr_file_remove(lua_State*);
int lua_apr_file_mtime_set(lua_State*);
int lua_apr_file_attrs_set(lua_State*);
int lua_apr_file_perms_set(lua_State*);
int lua_apr_stat(lua_State*);
int lua_apr_file_open(lua_State*);
lua_apr_file *file_alloc(lua_State*, const char*, lua_apr_pool*);
void init_file_buffers(lua_State*, lua_apr_file*, int);
lua_apr_file *file_check(lua_State*, int, int);
apr_status_t file_close_impl(lua_State*, lua_apr_file*);

/* io_net.c */
int lua_apr_socket_create(lua_State*);
int lua_apr_hostname_get(lua_State*);
int lua_apr_host_to_addr(lua_State*);
int lua_apr_addr_to_host(lua_State*);

/* io_pipe.c */
int lua_apr_pipe_open_stdin(lua_State*);
int lua_apr_pipe_open_stdout(lua_State*);
int lua_apr_pipe_open_stderr(lua_State*);
int lua_apr_namedpipe_create(lua_State*);
int lua_apr_pipe_create(lua_State*);

/* memory_pool.c */
apr_pool_t *to_pool(lua_State*);

/* object.c */
void *new_object(lua_State*, lua_apr_objtype*);
void *prepare_reference(lua_apr_objtype*, lua_apr_refobj*);
void init_object(lua_State*, lua_apr_objtype*);
int object_collectable(lua_apr_refobj*);
void release_object(lua_apr_refobj*);
void object_env_default(lua_State*);
int object_env_private(lua_State*, int);
int object_has_type(lua_State*, int, lua_apr_objtype*, int);
int objects_equal(lua_State*);
void *check_object(lua_State*, int, lua_apr_objtype*);
int get_metatable(lua_State*, lua_apr_objtype*);
void *object_incref(lua_apr_refobj*);
void *proxy_object(lua_State*, lua_apr_objtype*, lua_apr_refobj*);
int object_decref(void*);
lua_apr_pool *refpool_alloc(lua_State*);
apr_pool_t* refpool_incref(lua_apr_pool*);
void refpool_decref(lua_apr_pool*);

/* permissions.c */
int push_protection(lua_State*, apr_fileperms_t);
apr_fileperms_t check_permissions(lua_State*, int, int);

/* proc.c */
int lua_apr_proc_create(lua_State*);
int lua_apr_proc_detach(lua_State*);
int lua_apr_proc_fork(lua_State*);

/* stat.c */
void check_stat_request(lua_State*, lua_apr_stat_context*);
int push_stat_results(lua_State*, lua_apr_stat_context*, const char*);

/* shm.c */
int lua_apr_shm_create(lua_State*);
int lua_apr_shm_attach(lua_State*);
int lua_apr_shm_remove(lua_State*);

/* str.c */
int lua_apr_strnatcmp(lua_State*);
int lua_apr_strnatcasecmp(lua_State*);
int lua_apr_strfsize(lua_State*);
int lua_apr_tokenize_to_argv(lua_State*);

/* thread.c */
int lua_apr_thread_create(lua_State*);
int lua_apr_thread_yield(lua_State*);

/* thread_queue.c */
int lua_apr_thread_queue(lua_State*);

/* tuple.c */
int check_tuple(lua_State*, int, int, void**);
int push_tuple(lua_State*, void*);

/* time.c */
int lua_apr_sleep(lua_State*);
int lua_apr_time_now(lua_State*);
int lua_apr_time_explode(lua_State*);
int lua_apr_time_implode(lua_State*);
int lua_apr_time_format(lua_State*);
apr_time_t time_check(lua_State*, int);
int time_push(lua_State*, apr_time_t);

/* uri.c */
int lua_apr_uri_parse(lua_State*);
int lua_apr_uri_unparse(lua_State*);
int lua_apr_uri_port_of_scheme(lua_State*);

/* user.c */
int lua_apr_user_get(lua_State*);
int lua_apr_user_homepath_get(lua_State*);
int push_username(lua_State*, apr_pool_t*, apr_uid_t);
int push_groupname(lua_State*, apr_pool_t*, apr_gid_t);

/* uuid.c */
int lua_apr_uuid_get(lua_State*);
int lua_apr_uuid_format(lua_State*);
int lua_apr_uuid_parse(lua_State*);

/* xlate.c */
int lua_apr_xlate(lua_State*);

/* xml.c */
int lua_apr_xml(lua_State*);

/* memcache */
int lua_apr_memcache(lua_State *L);

#endif

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
