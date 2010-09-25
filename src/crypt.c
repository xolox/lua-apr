/* Cryptography routines module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: September 25, 2010
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * These functions support the [MD5] [md5] and [SHA1] [sha1] cryptographic hash
 * functions. You can also use them to encrypt plain text passwords using a
 * salt, validate plain text passwords against their encrypted, salted digest
 * and read passwords from standard input while masking the characters typed
 * by the user.
 *
 * The MD5 and SHA1 functions can be used to hash binary data. This is useful
 * because the hash is only 16 or 32 bytes long, yet it still changes
 * significantly when the binary data changes by just one byte.
 *
 * If that doesn't look useful consider the following scenario: The Lua authors
 * have just finished a new release of Lua and are about to publish the source
 * code on <http://lua.org>. Before they publish the .tar.gz archive they first
 * generate a hash of the archive. Then they publish both the archive and the
 * [hash] [lua_hashes]. Now when a user downloads the archive they can verify
 * whether it was corrupted or manipulated since it was published on
 * <http://lua.org> by comparing the published hash against the hash of the
 * archive they just downloaded.
 *
 * [md5]: http://en.wikipedia.org/wiki/MD5
 * [sha1]: http://en.wikipedia.org/wiki/SHA1
 * [lua_hashes]: http://www.lua.org/ftp/md5sum.out
 */

#include "lua_apr.h"
#include <apr_lib.h>
#include <apr_md5.h>
#include <apr_sha1.h>

static int format_digest(char*, const unsigned char*, int);

/* apr.md5(message [, binary]) -> digest {{{1
 *
 * Calculate the MD5 digest of the string @message. On success the digest is
 * returned as a string of 32 hexadecimal characters, or a string of 16 bytes
 * if @binary evaluates to true. Otherwise a nil followed by an error message
 * is returned.
 */

int lua_apr_md5(lua_State *L)
{
  unsigned char digest[APR_MD5_DIGESTSIZE];
  char formatted[APR_MD5_DIGESTSIZE*2 + 1];
  const char *message;
  apr_status_t status;
  size_t length;
  int binary;
  int pushed;

  message = luaL_checklstring(L, 1, &length);
  binary = lua_toboolean(L, 2);
  status = apr_md5(digest, message, (apr_size_t)length);

  if (status != APR_SUCCESS) {
    pushed = push_error_status(L, status);
  } else if (binary) {
    lua_pushlstring(L, (const char *)digest, count(digest));
    pushed = 1;
  } else if (format_digest(formatted, digest, count(digest))) {
    lua_pushlstring(L, (const char *)formatted, count(formatted) - 1);
    memset(formatted, 42, count(formatted));
    pushed = 1;
  } else {
    pushed = push_error_message(L, "could not format MD5 digest");
  }

  memset(digest, 42, count(digest));

  return pushed;
}

/* apr.md5_encode(password, salt) -> digest {{{1
 *
 * Encode the string @password using the MD5 algorithm and a @salt string. On
 * success the digest is returned. Otherwise a nil followed by an error message
 * is returned.
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
    lua_pushlstring(L, digest, count(digest));
    pushed = 1;
  }

  memset(digest, 42, count(digest));

  return pushed;
}

/* apr.sha1(message [, binary]) -> digest {{{1
 *
 * Calculate the SHA1 digest of the string @message. On success the digest is
 * returned as a string of 40 hexadecimal characters, or a string of 20 bytes
 * if @binary evaluates to true. Otherwise a nil followed by an error message
 * is returned.
 */

int lua_apr_sha1(lua_State *L)
{
  unsigned char digest[APR_SHA1_DIGESTSIZE];
  char formatted[APR_SHA1_DIGESTSIZE*2+1];
  apr_sha1_ctx_t context;
  const char *message;
  size_t length;
  int binary;
  int pushed;

  message = luaL_checklstring(L, 1, &length);
  binary = lua_toboolean(L, 2);

  apr_sha1_init(&context);
  apr_sha1_update(&context, message, (unsigned int)length);
  apr_sha1_final(digest, &context);

  if (binary) {
    lua_pushlstring(L, (const char *)digest, count(digest));
    pushed = 1;
  } else if (format_digest(formatted, digest, count(digest))) {
    lua_pushlstring(L, formatted, count(formatted) - 1);
    memset(formatted, 42, count(formatted));
    pushed = 1;
  } else {
    pushed = push_error_message(L, "could not format SHA1 digest");
  }

  memset(digest, 42, count(digest));

  return pushed;
}

/* apr.password_validate(password, digest) -> valid {{{1
 *
 * Validate the string @password against a @digest created by one of the
 * APR-supported algorithms (MD5 and SHA1). On success true is returned,
 * otherwise a nil followed by an error message is returned.
 *
 * Hashes created by crypt are supported only on platforms that provide
 * crypt(3), so don't rely on that function unless you know that your
 * application will be run only on platforms that support it. On platforms that
 * don't support crypt(3), this falls back to a clear text string comparison.
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
  apr_size_t length;
  char *password;
  int pushed;

  prompt = luaL_checkstring(L, 1);
  password = malloc(length = 512);
  if (password == NULL) {
    pushed = push_error_memory(L);
  } else {
    /* NOTE: apr_password_get() does NOT modify length */
    status = apr_password_get(prompt, password, &length);
    if (status != APR_SUCCESS) {
      pushed = push_error_status(L, status);
    } else {
      lua_pushstring(L, password);
      pushed = 1;
    }
  }
  memset(password, 42, length);
  free(password);

  return pushed;
}

/* }}}1 */

int format_digest(char *formatted, const unsigned char *digest, int length)
{
  int i;
  for (i = 0; i < length; i++)
    if (2 != sprintf(&formatted[i*2], "%02x", digest[i]))
      return 0;
  return 1;
}

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
