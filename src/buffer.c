/* Buffered I/O interface for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: October 17, 2010
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

#define CURSOR(B) (&B->data[B->index])
#define AVAIL(B) (B->index <= B->limit ? B->limit - B->index : 0)
#define SPACE(B) (B->limit <= B->size ? B->size - B->limit : 0)
#define SUCCESS_OR_EOF(S) ((S) == APR_SUCCESS || APR_STATUS_IS_EOF(S))

void init_buffers(lua_State *L, lua_apr_readbuf *input, lua_apr_writebuf *output, void *object, int text_mode, lua_apr_buf_rf read, lua_apr_buf_wf write, lua_apr_buf_ff flush) /* {{{1 */
{
#if !defined(WIN32) && !defined(OS2) && !defined(NETWARE)
  text_mode = 0;
#endif
  /* Initialize the input/read buffer's structure. */
  input->text_mode = text_mode;
  input->object = object;
  input->read = read;
  input->buffer.data = NULL;
  input->buffer.index = 0;
  input->buffer.limit = 0;
  input->buffer.size = 0;
  /* Initialize the output/write buffer's structure. */
  output->text_mode = text_mode;
  output->object = object;
  output->write = write;
  output->flush = flush;
  output->buffer.data = NULL;
  output->buffer.index = 0;
  output->buffer.limit = 0;
  output->buffer.size = 0;
}

void free_buffer(lua_State *L, lua_apr_buffer *B) /* {{{1 */
{
  if (B->data != NULL) {
    free(B->data);
    B->data = NULL;
    B->index = 0;
    B->limit = 0;
    B->size = 0;
  }
}

static int find_win32_eol(lua_apr_buffer *B, size_t offset, size_t *result) /* {{{1 */
{
  char *match, *cursor = CURSOR(B) + offset;
  size_t avail = AVAIL(B) - offset;

  match = memchr(cursor, '\r', avail);
  if (match != NULL) {
    size_t test = match - cursor;
    if (test < avail && *(match + 1) == '\n') {
      *result = test;
      return 1;
    }
  }
  return 0;
}

static void binary_to_text(lua_apr_buffer *B) /* {{{1 */
{
  size_t offset = 0, test;

  while (find_win32_eol(B, offset, &test)) {
    /* TODO binary_to_text() is very inefficient */
    size_t i = B->index + offset + test;
    char *p = &B->data[i];
    memmove(p, p + 1, B->limit - i);
    offset += test;
    B->limit--;
  }
}

static void shift_buffer(lua_apr_buffer *B) /* {{{1 */
{
  if (B->index > 0 && AVAIL(B) > 0) {
    memmove(B->data, CURSOR(B), AVAIL(B));
    B->limit = AVAIL(B);
    B->index = 0;
  } else if (AVAIL(B) == 0) {
    B->index = 0;
    B->limit = 0;
  }
}

static apr_status_t grow_buffer(lua_apr_buffer *B) /* {{{1 */
{
  apr_status_t status = APR_SUCCESS;
  size_t newsize = LUA_APR_BUFSIZE;
  char *newdata;

  if (B->size >= newsize)
    newsize = B->size / 2 * 3;
  newdata = realloc(B->data, newsize);
  if (newdata != NULL) {
    B->data = newdata;
    B->size = newsize;
  } else {
    status = APR_ENOMEM;
  }

  return status;
}

static apr_status_t fill_buffer(lua_apr_readbuf *input) /* {{{1 */
{
  lua_apr_buffer *B = &input->buffer;
  apr_status_t status;
  apr_size_t len;

  /* Shift the buffer's contents down? */
  shift_buffer(B);

  /* Try to grow the buffer? */
  if (!(B->limit < B->size))
    status = grow_buffer(B);

  /* Add more data to buffer. */
  len = SPACE(B);
  status = input->read(input->object, &B->data[B->limit], &len);
  if (status == APR_SUCCESS)
    B->limit += len;

  return status;
}

static apr_status_t read_line(lua_State *L, lua_apr_readbuf *input) /* {{{1 */
{
  lua_apr_buffer *B = &input->buffer;
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
      skip_crlf = (input->text_mode && length >= 1 && *(match - 1) == '\r');
      lua_pushlstring(L, CURSOR(B), skip_crlf ? length - 1 : length);
      B->index += length + 1;
      break;
    } else if (APR_STATUS_IS_EOF(status)) {
      /* Got EOF while searching for end of line? */
      if (AVAIL(B) >= 1) {
        lua_pushlstring(L, CURSOR(B), AVAIL(B));
        B->index += AVAIL(B);
      } else
        lua_pushnil(L);
      break;
    }
    /* Skip scanned input on next iteration. */
    offset = AVAIL(B);
    /* Get more input. */
    status = fill_buffer(input);
  } while (SUCCESS_OR_EOF(status));

  return status;
}

static apr_status_t read_number(lua_State *L, lua_apr_readbuf *input) /* {{{1 */
{
  lua_apr_buffer *B = &input->buffer;
  apr_status_t status = APR_SUCCESS;
  size_t offset = 0, test;
  lua_Number value;
  char *endptr;

  do {
    /* Always terminate the buffer with a NUL byte so that strspn() and
     * lua_str2number() won't scan past the end of the input buffer. */
    if (!(B->limit < B->size)) {
      status = grow_buffer(B);
      if (!(B->limit < B->size))
        break;
    }
    B->data[B->limit+1] = '\0';
    /* Skip leading whitespace in buffered input. */
    offset += strspn(CURSOR(B) + offset, " \n\t\r\f\v");
    /* Calculate available bytes but guard against overflow. */
    test = offset <= AVAIL(B) ? (AVAIL(B) - offset) : 0;
    /* Make sure enough input remains to read full number [or we got EOF]. */
    if (test >= LUAI_MAXNUMBER2STR || APR_STATUS_IS_EOF(status)) {
      /* Try to parse number at selected position. */
      value = lua_str2number(CURSOR(B) + offset, &endptr);
      if (endptr > CURSOR(B) + offset) {
        lua_pushnumber(L, value);
        B->index += endptr - CURSOR(B) + 1;
      } else
        lua_pushnil(L);
      break;
    }
    /* Get more input. */
    status = fill_buffer(input);
  } while (SUCCESS_OR_EOF(status));

  return status;
}

static apr_status_t read_chars(lua_State *L, lua_apr_readbuf *input, apr_size_t n) /* {{{1 */
{
  lua_apr_buffer *B = &input->buffer;
  apr_status_t status = APR_SUCCESS;

  /* XXX The <= comparison is intended to make sure we read enough bytes to
   * successfully convert \r\n to \n even when at the end of the buffer. */
  while (AVAIL(B) <= n && status == APR_SUCCESS)
    status = fill_buffer(input);

  if (AVAIL(B) > 0) {
    /* TODO binary_to_text() may not need to be called again! */
    if (n > 0 && input->text_mode)
      binary_to_text(B);
    if (n > AVAIL(B))
      n = AVAIL(B);
    lua_pushlstring(L, CURSOR(B), n);
    B->index += n;
  } else
    lua_pushnil(L);

  return status;
}

static apr_status_t read_all(lua_State *L, lua_apr_readbuf *input) /* {{{1 */
{
  lua_apr_buffer *B = &input->buffer;
  apr_status_t status;

  do {
    status = fill_buffer(input);
  } while (status == APR_SUCCESS);
  if (input->text_mode)
    binary_to_text(B);
  lua_pushlstring(L, CURSOR(B), AVAIL(B));
  B->index = B->limit;

  return status;
}

int read_buffer(lua_State *L, lua_apr_readbuf *B) /* {{{1 */
{
  apr_status_t status = APR_SUCCESS;
  int n, top, nresults = 0;

  top = lua_gettop(L);
  if (top == 1) {
    status = read_line(L, B);
    nresults = 1;
  } else {
    luaL_checkstack(L, top - 1, "too many arguments");
    for (n = 2; n <= top && status == APR_SUCCESS; n++) {
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
  if (!SUCCESS_OR_EOF(status)) {
    /* Replace results with (nil, message, code). */
    lua_settop(L, 1);
    nresults = push_error_status(L, status);
  }
  return nresults;
}

int write_buffer(lua_State *L, lua_apr_writebuf *output) /* {{{1 */
{
  lua_apr_buffer *B = &output->buffer;
  apr_status_t status = APR_SUCCESS;
  int i, add_eol, n = lua_gettop(L);
  size_t length, size;
  const char *data;
  char *match;

  if (B->data == NULL) { /* allocate write buffer on first use */
    B->data = malloc(LUA_APR_BUFSIZE);
    if (B->data == NULL)
      return APR_ENOMEM;
    B->size = LUA_APR_BUFSIZE;
  }

  for (i = 2; i <= n && status == APR_SUCCESS; i++) {
    data = luaL_checklstring(L, i, &length);
    while (length > 0 && status == APR_SUCCESS) {
      if (SPACE(B) > 0) { /* copy range of bytes to buffer? */
        size = length;
        add_eol = 0;
        if (size > SPACE(B)) { /* never write past end of buffer */
          size = SPACE(B);
        }
        if (output->text_mode) { /* copy no more than one line in text mode */
          match = memchr(data, '\n', size);
          if (match != NULL) {
            size = match - data; /* don't copy EOL directly */
            if (size + 2 <= SPACE(B))
              add_eol = 1; /* append EOL after copying line? */
          }
        }
        if (size > 0) { /* copy range of bytes to buffer! */
          memcpy(&B->data[B->limit], data, size);
          B->limit += size;
          data += size;
          length -= size;
        }
        if (add_eol) { /* add end of line sequence to buffer? */
          memcpy(&B->data[B->limit], "\r\n", 2);
          B->limit += 2;
          data += 1;
          length -= 1;
        }
      }
      if (AVAIL(B) > 0 && SPACE(B) <= 1) /* flush buffer contents? */
        status = flush_buffer(L, output, 1);
    }
  }

  return push_status(L, status);
}

apr_status_t flush_buffer(lua_State *L, lua_apr_writebuf *output, int soft) /* {{{1 */
{
  lua_apr_buffer *B = &output->buffer;
  apr_status_t status = APR_SUCCESS;
  apr_size_t len;

  /* Flush the internal write buffer. */
  while ((len = AVAIL(B)) > 0 && status == APR_SUCCESS) {
    status = output->write(output->object, CURSOR(B), &len);
    B->index += len;
  }

  /* Shift any remaining data down. */
  shift_buffer(B);

  /* Not sure whether deeper layers perform buffering of their own?! */
  if (status == APR_SUCCESS && !soft)
    status = output->flush(output->object);

  return status;
}
