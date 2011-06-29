/* Cryptography routines module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: June 30, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * These functions support the [MD5] [md5] and [SHA1] [sha1] cryptographic hash
 * functions. You can also use them to encrypt plain text passwords using a
 * [salt] [salt], validate plain text passwords against their encrypted, salted
 * digest and read passwords from standard input while masking the characters
 * typed by the user.
 *
 * The MD5 and SHA1 functions can be used to hash binary data. This is useful
 * because the hash is only 16 or 32 bytes long, yet it still changes
 * significantly when the binary data changes by just one byte.
 *
 * If that doesn't look useful consider the following scenario: The Lua authors
 * have just finished a new release of Lua and are about to publish the source
 * code on <http://lua.org>. Before they publish the [tarball] [tar] they first
 * calculate its MD5 and SHA1 hashes. They then publish the archive and hashes
 * on the [downloads page] [lua_downloads]. When a user downloads the tarball
 * they can verify whether it was corrupted or manipulated since it was
 * published on <http://lua.org> by comparing the published hash against the
 * hash of the tarball they just downloaded:
 *
 *     > handle = io.open('lua-5.1.4.tar.gz', 'rb')
 *     > data = handle:read('*a'); handle:close()
 *     > = apr.md5(data) == 'd0870f2de55d59c1c8419f36e8fac150'
 *     true
 *     > = apr.sha1(data) == '2b11c8e60306efb7f0734b747588f57995493db7'
 *     true
 *
 * [md5]: http://en.wikipedia.org/wiki/MD5
 * [sha1]: http://en.wikipedia.org/wiki/SHA1
 * [salt]: http://en.wikipedia.org/wiki/Salt_(cryptography)
 * [tar]: http://en.wikipedia.org/wiki/Tar_(file_format)
 * [lua_downloads]: http://www.lua.org/ftp/
 */

#include "lua_apr.h"
#include <apr_lib.h>
#include <apr_md5.h>
#include <apr_sha1.h>

/* Internal functions {{{1 */

/* The APR cryptography functions zero buffers before returning and the Lua/APR
 * binding does the same thing so that it doesn't negate the effort of APR. */
#define clear_mem(p, l) memset(p, 42, l)
#define clear_stack(b) clear_mem(b, sizeof(b))

typedef struct lua_apr_md5_ctx {
  lua_apr_refobj header;
  apr_md5_ctx_t context;
  int finalized;
} lua_apr_md5_ctx;

typedef struct lua_apr_sha1_ctx {
  lua_apr_refobj header;
  apr_sha1_ctx_t context;
  int finalized;
} lua_apr_sha1_ctx;

static int format_digest(char *formatted, const unsigned char *digest, int length)
{
  int i;
  for (i = 0; i < length; i++)
    if (2 != sprintf(&formatted[i*2], "%02x", digest[i]))
      return 0;
  return 1;
}

static lua_apr_md5_ctx *md5_check(lua_State *L, int idx, int valid)
{
  lua_apr_md5_ctx *context;
  context = check_object(L, idx, &lua_apr_md5_type);
  if (valid && context->finalized)
    luaL_error(L, "attempt to use a finalized MD5 context");
  return context;
}

static lua_apr_sha1_ctx *sha1_check(lua_State *L, int idx, int valid)
{
  lua_apr_sha1_ctx *context;
  context = check_object(L, idx, &lua_apr_sha1_type);
  if (valid && context->finalized)
    luaL_error(L, "attempt to use a finalized SHA1 context");
  return context;
}

/* apr.md5_encode(password, salt) -> digest {{{1
 *
 * Encode the string @password using the [MD5] [md5] algorithm and a [salt]
 * [salt] string. On success the digest is returned, otherwise a nil followed
 * by an error message is returned.
 */

int lua_apr_md5_encode(lua_State *L)
{
  const char *password, *salt;
  apr_status_t status;
  char digest[120];
  int pushed;

  password = luaL_checkstring(L, 1);
  salt = luaL_checkstring(L, 2);
  status = apr_md5_encode(password, salt, digest, count(digest));

  if (status != APR_SUCCESS) {
    pushed = push_error_status(L, status);
  } else {
    lua_pushstring(L, digest);
    pushed = 1;
  }

  clear_stack(digest);

  return pushed;
}

/* apr.password_validate(password, digest) -> valid {{{1
 *
 * Validate the string @password against a @digest created by one of the
 * APR-supported algorithms ([MD5] [md5] and [SHA1] [sha1]). On success true is
 * returned, otherwise a nil followed by an error message is returned.
 *
 * Hashes created by crypt are supported only on platforms that provide
 * [crypt(3)] [crypt_fun], so don't rely on that function unless you know that
 * your application will be run only on platforms that support it. On platforms
 * that don't support crypt(3), this falls back to a clear text string
 * comparison.
 *
 * [crypt_fun]: http://linux.die.net/man/3/crypt
 */

int lua_apr_password_validate(lua_State *L)
{
  const char *password, *digest;
  apr_status_t status;

  password = luaL_checkstring(L, 1);
  digest = luaL_checkstring(L, 2);
  status = apr_password_validate(password, digest);

  return push_status(L, status);
}

/* apr.password_get(prompt) -> password {{{1
 *
 * Display the string @prompt on the command-line prompt and read in a password
 * from standard input. If your platform allows it, the typed password will be
 * masked by a placeholder like `*`. On success the password is returned,
 * otherwise a nil followed by an error message is returned.
 */

int lua_apr_password_get(lua_State *L)
{
  apr_status_t status;
  const char *prompt;
  char password[256]; /* arbitrary limit */
  apr_size_t length = count(password);
  int pushed;

  prompt = luaL_checkstring(L, 1);
  /* Note that apr_password_get() does NOT modify length! :-S */
  status = apr_password_get(prompt, password, &length);
  if (status != APR_SUCCESS) {
    pushed = push_error_status(L, status);
  } else {
    lua_pushstring(L, password);
    pushed = 1;
  }
  clear_stack(password);

  return pushed;
}

/* apr.md5_init() -> md5_context {{{1
 *
 * Create and return an object that can be used to calculate [MD5] [md5]
 * message digests in steps. If an error occurs a nil followed by an error
 * message is returned. This can be useful when you want to calculate message
 * digests of large inputs, for example files like [ISO images] [isoimg] and
 * backups:
 *
 *     > function md5_file(path, binary)
 *     >>  local handle = assert(io.open(path, 'rb'))
 *     >>  local context = assert(apr.md5_init())
 *     >>  while true do
 *     >>    local block = handle:read(1024 * 1024)
 *     >>    if not block then break end
 *     >>    assert(context:update(block))
 *     >>  end
 *     >>  return context:digest(binary)
 *     >> end
 *     >
 *     > md5_file 'ubuntu-10.04-desktop-i386.iso'
 *     'd044a2a0c8103fc3e5b7e18b0f7de1c8'
 *
 * [isoimg]: http://en.wikipedia.org/wiki/ISO_image
 */

int lua_apr_md5_init(lua_State *L)
{
  apr_status_t status;
  lua_apr_md5_ctx *object;

  object = new_object(L, &lua_apr_md5_type);
  if (object == NULL)
    return push_error_memory(L);
  status = apr_md5_init(&object->context);
  if (status != APR_SUCCESS)
    return push_error_status(L, status);

  return 1;
}

/* md5_context:update(input) -> status {{{1
 *
 * Continue an [MD5] [md5] message digest operation by processing another
 * message block and updating the context. On success true is returned,
 * otherwise a nil followed by an error message is returned.
 */

static int md5_update(lua_State *L)
{
  apr_status_t status;
  lua_apr_md5_ctx *object;
  const char *input;
  size_t length;

  object = md5_check(L, 1, 1);
  input = luaL_checklstring(L, 2, &length);
  status = apr_md5_update(&object->context, input, length);

  return push_status(L, status);
}

/* md5_context:digest([binary]) -> digest {{{1
 *
 * End an [MD5] [md5] message digest operation. On success the digest is
 * returned as a string of 32 hexadecimal characters, or a string of 16 bytes
 * if @binary evaluates to true. Otherwise a nil followed by an error message
 * is returned.
 *
 * If you want to re-use the context object after calling this method
 * see `md5_context:reset()`.
 */

static int md5_digest(lua_State *L)
{
  apr_status_t status;
  lua_apr_md5_ctx *object;
  unsigned char digest[APR_MD5_DIGESTSIZE];
  char formatted[APR_MD5_DIGESTSIZE*2 + 1];
  int binary, pushed = 1;

  object = md5_check(L, 1, 1);
  binary = lua_toboolean(L, 2);
  status = apr_md5_final(digest, &object->context);
  if (status != APR_SUCCESS) {
    pushed = push_error_status(L, status);
  } else if (binary) {
    lua_pushlstring(L, (const char *)digest, count(digest));
  } else if (format_digest(formatted, digest, count(digest))) {
    lua_pushlstring(L, (const char *)formatted, count(formatted) - 1);
    clear_stack(formatted);
  } else {
    pushed = push_error_message(L, "could not format MD5 digest");
  }
  clear_stack(digest);
  object->finalized = 1;

  return pushed;
}

/* md5_context:reset() -> status {{{1
 *
 * Use this method to reset the context after calling `md5_context:digest()`.
 * This enables you to re-use the same context to perform another message
 * digest calculation. On success true is returned, otherwise a nil followed by
 * an error message is returned.
 */

static int md5_reset(lua_State *L)
{
  apr_status_t status;
  lua_apr_md5_ctx *object;

  object = md5_check(L, 1, 0);
  status = apr_md5_init(&object->context);
  if (status == APR_SUCCESS)
    object->finalized = 0;

  return push_status(L, status);
}

/* md5_context:__tostring() {{{1 */

static int md5_tostring(lua_State *L)
{
  lua_apr_md5_ctx *object;

  object = md5_check(L, 1, 0);
  if (!object->finalized)
    lua_pushfstring(L, "%s (%p)", lua_apr_md5_type.friendlyname, object);
  else
    lua_pushfstring(L, "%s (closed)", lua_apr_md5_type.friendlyname);

  return 1;
}

/* md5_context:__gc() {{{1 */

static int md5_gc(lua_State *L)
{
  lua_apr_md5_ctx *object = md5_check(L, 1, 0);
  release_object((lua_apr_refobj*)object);
  return 1;
}

/* apr.sha1_init() -> sha1_context {{{1
 *
 * Create and return an object that can be used to calculate [SHA1] [sha1]
 * message digests in steps. See also the example for `apr.md5_init()`.
 */

int lua_apr_sha1_init(lua_State *L)
{
  lua_apr_sha1_ctx *object;

  object = new_object(L, &lua_apr_sha1_type);
  if (object == NULL)
    return push_error_memory(L);
  apr_sha1_init(&object->context);

  return 1;
}

/* sha1_context:update(input) -> status {{{1
 *
 * Continue an [SHA1] [sha1] message digest operation by processing another
 * message block and updating the context.
 */

static int sha1_update(lua_State *L)
{
  lua_apr_sha1_ctx *object;
  const char *input;
  size_t length;

  object = sha1_check(L, 1, 1);
  input = luaL_checklstring(L, 2, &length);
  apr_sha1_update(&object->context, input, length);

  return push_status(L, APR_SUCCESS);
}

/* sha1_context:digest([binary]) -> digest {{{1
 *
 * End an [SHA1] [sha1] message digest operation. On success the digest is
 * returned as a string of 40 hexadecimal characters, or a string of 20 bytes
 * if @binary evaluates to true. Otherwise a nil followed by an error message
 * is returned.
 *
 * If you want to re-use the context object after calling this method
 * see `sha1_context:reset()`.
 */

static int sha1_digest(lua_State *L)
{
  lua_apr_sha1_ctx *object;
  unsigned char digest[APR_SHA1_DIGESTSIZE];
  char formatted[APR_SHA1_DIGESTSIZE*2 + 1];
  int binary, pushed = 1;

  object = sha1_check(L, 1, 1);
  binary = lua_toboolean(L, 2);
  apr_sha1_final(digest, &object->context);
  if (binary) {
    lua_pushlstring(L, (const char *)digest, count(digest));
  } else if (format_digest(formatted, digest, count(digest))) {
    lua_pushlstring(L, (const char *)formatted, count(formatted) - 1);
    clear_stack(formatted);
  } else {
    pushed = push_error_message(L, "could not format SHA1 digest");
  }
  clear_stack(digest);
  object->finalized = 1;

  return pushed;
}

/* sha1_context:reset() -> status {{{1
 *
 * Use this method to reset the context after calling `sha1_context:digest()`.
 * This enables you to re-use the same context to perform another message
 * digest calculation.
 */

static int sha1_reset(lua_State *L)
{
  lua_apr_sha1_ctx *object;

  object = sha1_check(L, 1, 0);
  apr_sha1_init(&object->context);
  object->finalized = 0;

  return push_status(L, APR_SUCCESS);
}

/* sha1_context:__tostring() {{{1 */

static int sha1_tostring(lua_State *L)
{
  lua_apr_sha1_ctx *object;

  object = sha1_check(L, 1, 0);
  if (!object->finalized)
    lua_pushfstring(L, "%s (%p)", lua_apr_sha1_type.friendlyname, object);
  else
    lua_pushfstring(L, "%s (closed)", lua_apr_sha1_type.friendlyname);

  return 1;
}

/* sha1_context:__gc() {{{1 */

static int sha1_gc(lua_State *L)
{
  lua_apr_sha1_ctx *object = sha1_check(L, 1, 0);
  release_object((lua_apr_refobj*)object);
  return 1;
}

/* }}}1 */

static luaL_reg md5_methods[] = {
  { "reset", md5_reset },
  { "update", md5_update },
  { "digest", md5_digest },
  { NULL, NULL },
};

static luaL_reg md5_metamethods[] = {
  { "__tostring", md5_tostring },
  { "__eq", objects_equal },
  { "__gc", md5_gc },
  { NULL, NULL },
};

lua_apr_objtype lua_apr_md5_type = {
  "lua_apr_md5_ctx*",      /* metatable name in registry */
  "md5 context",           /* friendly object name */
  sizeof(lua_apr_md5_ctx), /* structure size */
  md5_methods,             /* methods table */
  md5_metamethods          /* metamethods table */
};

static luaL_reg sha1_methods[] = {
  { "reset", sha1_reset },
  { "update", sha1_update },
  { "digest", sha1_digest },
  { NULL, NULL },
};

static luaL_reg sha1_metamethods[] = {
  { "__tostring", sha1_tostring },
  { "__eq", objects_equal },
  { "__gc", sha1_gc },
  { NULL, NULL },
};

lua_apr_objtype lua_apr_sha1_type = {
  "lua_apr_sha1_ctx*",      /* metatable name in registry */
  "sha1 context",           /* friendly object name */
  sizeof(lua_apr_sha1_ctx), /* structure size */
  sha1_methods,             /* methods table */
  sha1_metamethods          /* metamethods table */
};

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
