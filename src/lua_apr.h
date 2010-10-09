/* Header file for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: October 4, 2010
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 */

/* Global headers. {{{1 */
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <apr.h>
#include <apu.h>
#include <apr_general.h>
#include <apr_file_info.h>
#include <apr_file_io.h>
#include <apr_thread_proc.h>

/* Macro definitions. {{{1 */

#define LUA_APR_DBG(MSG, ...) \
  fprintf(stderr, MSG, __VA_ARGS__)

/*
#define LUA_APR_DBG(MSG, ...) \
  fprintf(stderr, "%s:%i@%s(): " MSG, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
*/

/* FIXME Pushing onto the stack might not work in this case? */
#define error_message_memory "memory allocation error"

#define LUA_APR_BUFSIZE 1024

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

#define push_error_status(L, status) \
  (lua_pushnil((L)), status_to_message((L), (status)), \
   lua_pushinteger((L), (status)), 3)

#define push_error_memory(L) \
  push_error_message((L), (error_message_memory))

/* Type definitions. {{{1 */

/* Context structure for stat() calls. */
typedef struct lua_apr_stat_context {
  apr_int32_t wanted;
  apr_finfo_t info;
  apr_int32_t fields[15];
  int firstarg, lastarg, count;
} lua_apr_stat_context;

typedef enum lua_apr_stat_result {
  STAT_DEFAULT_TABLE,
  STAT_DEFAULT_PATH
} lua_apr_stat_result;

/* Structure for directory objects. */
typedef struct lua_apr_dir {
  apr_dir_t *handle;
  apr_pool_t *memory_pool;
  const char *filepath;
} lua_apr_dir;

/* Structure for internal I/O buffers. */
typedef apr_status_t (*lua_apr_buffer_rf)(void*, char*, apr_size_t*);
typedef apr_status_t (*lua_apr_buffer_wf)(void*, const char*, apr_size_t*);
typedef struct lua_apr_buffer {
  char *input;
  size_t index, limit, size;
  void *object;
  int text_mode;
  lua_apr_buffer_rf read;
  lua_apr_buffer_wf write;
} lua_apr_buffer;

/* Structure for file objects. */
typedef struct lua_apr_file {
  lua_apr_buffer buffer;
  apr_file_t *handle;
  apr_pool_t *memory_pool;
  const char *path;
} lua_apr_file;

/* Structure for Lua userdatum types. */
typedef struct lua_apr_type {
  const char *typename;
  const size_t objsize;
  luaL_Reg *methods;
  luaL_Reg *metamethods;
} lua_apr_type;

/* External type definitions. */
extern lua_apr_type lua_apr_dir_type;
extern lua_apr_type lua_apr_file_type;

/* Prototypes. {{{1 */

/* lua_apr.c */
apr_pool_t *to_pool(lua_State*);
int get_metatable(lua_State*, lua_apr_type*);
int lua_apr_platform_get(lua_State*);
int push_status(lua_State*, apr_status_t);
int status_to_message(lua_State*, apr_status_t);
void *check_object(lua_State*, int, lua_apr_type*);
void *new_object(lua_State*, lua_apr_type*);

/* base64.c */
int lua_apr_base64_encode(lua_State*);
int lua_apr_base64_decode(lua_State*);

/* buffer.c */
void init_buffer(lua_State*, lua_apr_buffer*, void*, int, lua_apr_buffer_rf, lua_apr_buffer_wf);
int read_buffer(lua_State*, lua_apr_buffer*);
int write_buffer(lua_State*, lua_apr_buffer*);
void free_buffer(lua_State*, lua_apr_buffer*);

/* crypt.c */
int lua_apr_md5(lua_State*);
int lua_apr_md5_encode(lua_State*);
int lua_apr_sha1(lua_State*);
int lua_apr_password_validate(lua_State*);
int lua_apr_password_get(lua_State*);

/* env.c */
int lua_apr_env_get(lua_State*);
int lua_apr_env_set(lua_State*);
int lua_apr_env_delete(lua_State*);

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

/* io_dir.c */
int lua_apr_temp_dir_get(lua_State*);
int lua_apr_dir_make(lua_State*);
int lua_apr_dir_make_recursive(lua_State*);
int lua_apr_dir_remove(lua_State*);
int lua_apr_dir_remove_recursive(lua_State*);
int lua_apr_dir_open(lua_State*);
int dir_close(lua_State*);
int dir_entries(lua_State*);
int dir_read(lua_State*);
int dir_remove(lua_State*);
int dir_rewind(lua_State*);
int dir_gc(lua_State*);
int dir_tostring(lua_State*);

/* io_file.c */
int lua_apr_file_copy(lua_State*);
int lua_apr_file_append(lua_State*);
int lua_apr_file_rename(lua_State*);
int lua_apr_file_remove(lua_State*);
int lua_apr_file_mtime_set(lua_State*);
int lua_apr_file_attrs_set(lua_State*);
int lua_apr_stat(lua_State*);
int lua_apr_file_open(lua_State*);
int file_stat(lua_State*);
int file_read(lua_State*);
int file_write(lua_State*);
int file_seek(lua_State*);
int file_flush(lua_State*);
int file_lock(lua_State*);
int file_unlock(lua_State*);
int file_close(lua_State*);
int file_gc(lua_State*);
int file_tostring(lua_State*);
lua_apr_file *file_check(lua_State*, int, int);

/* permissions.c */
int push_protection(lua_State*, apr_fileperms_t);
apr_fileperms_t check_permissions(lua_State*, int, int);

/* stat.c */
void check_stat_request(lua_State*, lua_apr_stat_context*, lua_apr_stat_result);
int push_stat_results(lua_State*, lua_apr_stat_context*, const char*);

/* str.c */
int lua_apr_strnatcmp(lua_State*);
int lua_apr_strnatcasecmp(lua_State*);
int lua_apr_strfsize(lua_State*);
int lua_apr_tokenize_to_argv(lua_State*);

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

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
