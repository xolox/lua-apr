/* Buffered I/O interface for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: October 9, 2010
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * Notes:
 *  - The read_*() functions keep their own relative `offset' instead of
 *    advancing buffer.index because reading might fail and fill_buffer()
 *    invalidates absolute buffer indexes when it shifts down the buffer.
 *  - read_buffer() was based on the following references:
 *     - http://www.lua.org/source/5.1/liolib.c.html#g_read 
 *     - http://www.lua.org/manual/5.1/manual.html#pdf-file:read
 */

#include "lua_apr.h"

#define CURSOR(B) (&B->input.data[B->input.index])
#define AVAIL(B) (B->input.limit - B->input.index)
#define SUCCESS_OR_EOF(S) (S == APR_SUCCESS || APR_STATUS_IS_EOF(S))

void init_buffer(lua_State *L, lua_apr_buf *B, void *object, int text_mode, lua_apr_buf_rf read, lua_apr_buf_wf write) /* {{{1 */
{
  /* FIXME Don't initialize read buffer until first use! */
  B->input.data = malloc(LUA_APR_BUFSIZE);
  B->input.index = 0;
  B->input.limit = 0;
  B->input.size = LUA_APR_BUFSIZE;
  B->object = object;
  B->read = read;
  B->write = write;
  B->text_mode = text_mode;
}

void free_buffer(lua_State *L, lua_apr_buf *B) /* {{{1 */
{
  if (B->input.data != NULL) {
    free(B->input.data);
    B->input.data = NULL;
    B->input.index = 0;
    B->input.limit = 0;
    B->input.size = 0;
  }
}

static int find_win32_eol(lua_apr_buf *B, size_t offset, size_t *result) /* {{{1 */
{
  char *match;
  match = memchr(CURSOR(B) + offset, '\r', AVAIL(B) - offset);
  if (match != NULL) {
    size_t test = match - (CURSOR(B) + offset);
    if (test < AVAIL(B) - offset && *(match + 1) == '\n') {
      *result = test;
      return 1;
    }
  }
  return 0;
}

static void binary_to_text(lua_apr_buf *B) /* {{{1 */
{
  size_t offset = 0, test;

  while (find_win32_eol(B, offset, &test)) {
    /* FIXME Very inefficient but should work? */
    size_t i = B->input.index + offset + test;
    char *p = &B->input.data[i];
    memmove(p, p + 1, B->input.limit - i);
    offset += test;
    B->input.limit--;
  }
}

static apr_status_t grow_buffer(lua_apr_buf *B) /* {{{1 */
{
  apr_status_t status = APR_SUCCESS;

  /* FIXME This can't currently be used to resize the write buffer :-( */

  if (B->input.limit == B->input.size) {
    size_t newsize = B->input.size / 2 * 3;
    char *temp = realloc(B->input.data, newsize);
    if (temp != NULL) {
      B->input.data = temp;
      B->input.size = newsize;
    } else {
      /* FIXME Should this raise an error?! */
      status = APR_ENOMEM;
    }
  }

  return status;
}

static apr_status_t fill_buffer(lua_apr_buf *B) /* {{{1 */
{
  apr_status_t status;
  apr_size_t len;

  /* Shift the buffer's contents down? */
  if (B->input.index > 0 && AVAIL(B) > 0) {
    memmove(B->input.data, CURSOR(B), AVAIL(B));
    B->input.limit -= B->input.index;
    B->input.index = 0;
  }

  /* Try to grow the buffer? */
  status = grow_buffer(B);

  /* Add more data to buffer. */
  len = B->input.size - B->input.limit;
  status = B->read(B->object, &B->input.data[B->input.limit], &len);
  if (status == APR_SUCCESS)
    B->input.limit += len;

  return status;
}

static apr_status_t read_line(lua_State *L, lua_apr_buf *B) /* {{{1 */
{
  apr_status_t status = APR_SUCCESS;
  size_t offset = 0, length;
  int skip_crlf;
  char *match;

  do {
    /* Scan buffered input for line feed (LF) character. */
    match = memchr(CURSOR(B) + offset, '\n', AVAIL(B) - offset);
    if (match != NULL) {
      /* Found a line feed character! */
      length = match - CURSOR(B);
      /* Check for preceding carriage return (CR) character. */
      skip_crlf = (B->text_mode && length >= 1 && *(match - 1) == '\r');
      lua_pushlstring(L, CURSOR(B), skip_crlf ? length - 1 : length);
      B->input.index += length + 1;
      break;
    } else if (APR_STATUS_IS_EOF(status)) {
      /* Got EOF while searching for end of line? */
      if (AVAIL(B) >= 1) {
        lua_pushlstring(L, CURSOR(B), AVAIL(B));
        B->input.index += AVAIL(B);
      } else
        lua_pushnil(L);
      break;
    }
    /* Skip scanned input on next iteration. */
    offset = AVAIL(B);
    /* Get more input. */
    status = fill_buffer(B);
  } while (SUCCESS_OR_EOF(status));

  return status;
}

static apr_status_t read_number(lua_State *L, lua_apr_buf *B) /* {{{1 */
{
  apr_status_t status = APR_SUCCESS;
  size_t offset = 0;
  lua_Number value;
  char *endptr;

  do {
    /* Always terminate the buffer with a NUL byte so that strspn() and
     * lua_str2number() won't scan past the end of the input buffer. */
    if (!(B->input.limit < B->input.size)) {
      status = grow_buffer(B);
      if (!(B->input.limit < B->input.size))
        break;
    }
    B->input.data[B->input.limit+1] = '\0';
    /* Skip leading whitespace in buffered input. */
    offset += strspn(CURSOR(B) + offset, " \n\t\r\f\v");
    /* Make sure enough input remains to read a full number [or we got EOF]. */
    if (AVAIL(B) - offset >= LUAI_MAXNUMBER2STR || APR_STATUS_IS_EOF(status)) {
      /* Try to parse number at selected position. */
      value = lua_str2number(CURSOR(B) + offset, &endptr);
      if (endptr > CURSOR(B) + offset) {
        lua_pushnumber(L, value);
        B->input.index += endptr - CURSOR(B) + 1;
      } else
        lua_pushnil(L);
      break;
    }
    /* Get more input. */
    status = fill_buffer(B);
  } while (SUCCESS_OR_EOF(status));

  return status;
}

static apr_status_t read_chars(lua_State *L, lua_apr_buf *B, apr_size_t n) /* {{{1 */
{
  apr_status_t status = APR_SUCCESS;

  while (AVAIL(B) < n && status == APR_SUCCESS)
    status = fill_buffer(B);

  if (AVAIL(B) > 0) {
    if (n > AVAIL(B))
      n = AVAIL(B);
    /* FIXME Don't search buffer again on every call! */
    if (n > 0 && B->text_mode)
      binary_to_text(B);
    lua_pushlstring(L, CURSOR(B), n);
    B->input.index += n;
  } else
    lua_pushnil(L);

  return status;
}

static apr_status_t read_all(lua_State *L, lua_apr_buf *B) /* {{{1 */
{
  apr_status_t status;

  do {
    status = fill_buffer(B);
  } while (status == APR_SUCCESS);
  if (B->text_mode)
    binary_to_text(B);
  lua_pushlstring(L, CURSOR(B), AVAIL(B));
  B->input.index = B->input.limit;

  return status;
}

int read_buffer(lua_State *L, lua_apr_buf *B) /* {{{1 */
{
  apr_status_t status = APR_SUCCESS;
  int n, nargs, nresults = 0;

  nargs = lua_gettop(L) - 1;
  if (nargs == 0) {
    status = read_line(L, B);
    nresults = 1;
  } else {
    luaL_checkstack(L, nargs, "too many arguments");
    for (n = 2; n <= nargs + 1 && status == APR_SUCCESS; n++) {
      if (lua_type(L, n) == LUA_TNUMBER) { /* read given number of bytes */
        status = read_chars(L, B, (apr_size_t)lua_tointeger(L, n));
        nresults++;
      } else {
        const char *p = lua_tostring(L, n);
        luaL_argcheck(L, p && p[0] == '*', n, "invalid option");
        switch (p[1]) {
          case 'n': /* read number */
            status = read_number(L, B);
            nresults++;
            break;
          case 'l': /* read line */
            status = read_line(L, B);
            nresults++;
            break;
          case 'a': /* read remaining data */
            status = read_all(L, B);
            nresults++;
            break;
          default:
            return luaL_argerror(L, n, "invalid format");
        }
      }
    }
  }
  if (!SUCCESS_OR_EOF(status))
    raise_error_status(L, status);
# if 0
    lua_pop(L, 1); /* remove last result */
    lua_pushnil(L); /* push nil instead */
# endif
  return nresults;
}

int write_buffer(lua_State *L, lua_apr_buf *B) /* {{{1 */
{
  apr_status_t status = APR_SUCCESS;
  const char *data;
  size_t length;
  apr_size_t len;
  int i, n = lua_gettop(L);

  /* TODO Buffered writing + text mode filter! */

  for (i = 2; i <= n && status == APR_SUCCESS; i++) {
    data = luaL_checklstring(L, i, &length);
    len = length;
    status = B->write(B->object, data, &len);
  }

  return push_status(L, status);
}
