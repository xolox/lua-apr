/* Buffered I/O interface for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: June 16, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * Notes:
 *  - Since the addition of the buffer.unmanaged property for shared memory
 *    segments, "buffered I/O interface" has really become a misnomer :-)
 *  - The read_*() functions keep their own relative `offset' instead of
 *    advancing buffer.index because reading might fail and fill_buffer()
 *    invalidates absolute buffer indexes when it shifts down the buffer.
 *  - read_buffer() was based on the following references:
 *     - http://www.lua.org/source/5.1/liolib.c.html#g_read
 *     - http://www.lua.org/manual/5.1/manual.html#pdf-file:read
 *
 * Status:
 * I don't like this code at all but it's a core part of the value in the
 * Lua/APR binding! The code is quite confusing though and new bugs keep
 * cropping up. Things that should at least be fixed:
 *
 * TODO strspn() is incompatible with unmanaged buffers because it can scan
 * past the end of the buffer when the input is not terminated with a NUL
 * byte (and doing so is out of the question for e.g. shared memory).
 *
 * TODO lua_str2number() can scan past the end of the input buffer but this
 * is easy to fix: just copy MAX(AVAIL(B), LUAI_MAXNUMBER2STR) bytes to a
 * static buffer and terminate that buffer with a NUL byte :-)
 *
 * TODO Binary to text translation is currently impossible in unmanaged
 * buffers (shared memory). Decide whether to document or fix this issue?
 *
 * TODO The various read functions don't return immediately when status !=
 * (APR_SUCCESS, APR_EOF) while they probably should?
 */

#include "lua_apr.h"

/* Subtract a from b without producing negative values. */
#define SAFE_SUB(a, b) ((a) <= (b) ? (b) - (a) : 0)

#define CURSOR(B) (&B->data[B->index])
#define AVAIL(B) SAFE_SUB(B->index, B->limit)
#define SPACE(B) (B->unmanaged ? SAFE_SUB(B->index, B->size) : SAFE_SUB(B->limit, B->size))
#define SUCCESS_OR_EOF(B, S) ((S) == APR_SUCCESS || CHECK_FOR_EOF(B, S))
#define CHECK_FOR_EOF(B, S) (APR_STATUS_IS_EOF(S) || (B)->unmanaged)
#define DEBUG_BUFFER(B) do { \
  LUA_APR_DBG("buffer.index = %i", (B)->index); \
  LUA_APR_DBG("buffer.limit = %i", (B)->limit); \
  LUA_APR_DBG("buffer.size  = %i (really %i)", (B)->size, (B)->size + LUA_APR_BUFSLACK); \
} while (0)

/* Internal functions. {{{1 */

/* find_win32_eol() {{{2 */

static int find_win32_eol(lua_apr_buffer *B, size_t offset, size_t *result)
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

/* binary_to_text() {{{2 */

static void binary_to_text(lua_apr_buffer *B)
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

/* shift_buffer() {{{2 */

static void shift_buffer(lua_apr_buffer *B)
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

/* grow_buffer() {{{2 */

static apr_status_t grow_buffer(lua_apr_buffer *B)
{
  apr_status_t status = APR_SUCCESS;
  size_t newsize = LUA_APR_BUFSIZE;
  char *newdata;

  /* Don't do anything for unmanaged buffers. */
  if (B->unmanaged)
    return status;

  if (B->size >= newsize)
    newsize = B->size / 2 * 3;
  newdata = realloc(B->data, newsize + LUA_APR_BUFSLACK);
  if (newdata != NULL) {
    B->data = newdata;
    B->size = newsize;
    /* TODO Initialize new space to all zero bytes to make Valgrind happy?
    memset(&B->data[B->limit + 1], 0, B->size + LUA_APR_BUFSLACK - B->limit - 1); */
  } else {
    status = APR_ENOMEM;
  }

  return status;
}

/* fill_buffer() {{{2 */

static apr_status_t fill_buffer(lua_apr_readbuf *input, apr_size_t len)
{
  lua_apr_buffer *B = &input->buffer;
  apr_status_t status = APR_SUCCESS;

  /* Don't do anything for unmanaged buffers. */
  if (B->unmanaged)
    return status;

  /* Shift the buffer's contents down? */
  shift_buffer(B);

  /* Try to grow the buffer? */
  if (!(B->limit < B->size))
    status = grow_buffer(B);

  /* Add more data to buffer. */
  len -= AVAIL(B);
  if (len > SPACE(B))
    len = SPACE(B);
  status = input->read(input->object, &B->data[B->limit], &len);
  if (status == APR_SUCCESS)
    B->limit += len;

  return status;
}

/* read_line() {{{2 */

static apr_status_t read_line(lua_State *L, lua_apr_readbuf *input)
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
    } else if (CHECK_FOR_EOF(&input->buffer, status)) {
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
    status = fill_buffer(input, LUA_APR_BUFSIZE);
  } while (SUCCESS_OR_EOF(&input->buffer, status));

  return status;
}

/* read_number() {{{2 */

static apr_status_t read_number(lua_State *L, lua_apr_readbuf *input)
{
  lua_apr_buffer *B = &input->buffer;
  apr_status_t status = APR_SUCCESS;
  size_t offset = 0, test;
  lua_Number value;
  char *endptr;

  do {
    /* Make sure there is space in the buffer for a terminating NUL byte
     * (unmanaged buffers have LUA_APR_BUFSLACK extra bytes available). */
    if (!(B->unmanaged || B->limit < B->size)) {
      status = grow_buffer(B);
      /* TODO What's the best way to handle OOM? */
      if (!(B->limit < B->size))
        break;
    }
    /* Terminate the buffer with a NUL byte so that strspn() and
     * lua_str2number() don't scan past the end of the input buffer! */
    B->data[B->limit + 1] = '\0';
    /* Skip any leading whitespace in the buffered input. */
    offset += strspn(CURSOR(B) + offset, " \n\t\r\f\v");
    /* Calculate available bytes but guard against overflow. */
    test = SAFE_SUB(offset, AVAIL(B));
    /* Make sure enough input remains to read full number [or we got EOF]. */
    if (test >= LUAI_MAXNUMBER2STR || CHECK_FOR_EOF(B, status)) {
      if (test > 0) {
        /* Try to parse number at selected position. */
        value = lua_str2number(CURSOR(B) + offset, &endptr);
        if (endptr > CURSOR(B) + offset) {
          lua_pushnumber(L, value);
          B->index += endptr - CURSOR(B) + 1;
          break;
        }
      }
      lua_pushnil(L);
      break;
    }
    /* Get more input. */
    status = fill_buffer(input, LUA_APR_BUFSIZE);
  } while (SUCCESS_OR_EOF(&input->buffer, status));

  return status;
}

/* read_chars() {{{2 */

static apr_status_t read_chars(lua_State *L, lua_apr_readbuf *input, apr_size_t n)
{
  lua_apr_buffer *B = &input->buffer;
  apr_status_t status = APR_SUCCESS;
  int cornercase;

  for (;;) {
    /* Return nil on EOF (this only makes sense after the 1st iteration). */
    if (CHECK_FOR_EOF(B, status) && AVAIL(B) == 0) {
      lua_pushnil(L);
      break;
    }
    /* Check if we have enough input or reached EOF with buffered input. */
    cornercase = input->text_mode && B->data[SAFE_SUB(1, AVAIL(B))] == '\r';
    if ((n <= AVAIL(B) && !cornercase) || CHECK_FOR_EOF(B, status)) {
      if (n > AVAIL(B))
        n = AVAIL(B);
      lua_pushlstring(L, CURSOR(B), n);
      B->index += n;
      break;
    }
    /* Get more input? */
    if (!B->unmanaged) {
      status = fill_buffer(input, n + cornercase);
      if (!SUCCESS_OR_EOF(B, status))
        break;
      if (input->text_mode) /* && (n + cornercase) > 0 */
        binary_to_text(B);
    }
  }

  return status;
}

/* read_all() {{{2 */

static apr_status_t read_all(lua_State *L, lua_apr_readbuf *input)
{
  lua_apr_buffer *B = &input->buffer;
  apr_status_t status = APR_SUCCESS;

  if (!B->unmanaged) {
    do {
      status = fill_buffer(input, APR_SIZE_MAX);
    } while (status == APR_SUCCESS);
    if (input->text_mode)
      binary_to_text(B);
  }
  lua_pushlstring(L, CURSOR(B), AVAIL(B));
  B->index = B->limit + 1;

  return status;
}

/* read_lines_cb() {{{2 */

static int read_lines_cb(lua_State *L)
{
  lua_apr_readbuf *B;
  apr_status_t status;

  B = lua_touserdata(L, lua_upvalueindex(1));
  status = read_line(L, B);
  if (status == APR_SUCCESS)
    return 1;
  else if (CHECK_FOR_EOF(&B->buffer, status))
    return 0;
  else
    return push_error_status(L, status);
}

/* init_buffers() {{{1 */

void init_buffers(lua_State *L,
    lua_apr_readbuf *input,
    lua_apr_writebuf *output,
    void *object, int text_mode,
    lua_apr_buf_rf read,
    lua_apr_buf_wf write,
    lua_apr_buf_ff flush)
{
#if !defined(WIN32) && !defined(OS2) && !defined(NETWARE)
  text_mode = 0;
#endif
  /* Initialize the input buffer structure. */
  input->text_mode = text_mode;
  input->object = object;
  input->read = read;
  input->buffer.unmanaged = 0;
  input->buffer.data = NULL;
  input->buffer.index = 0;
  input->buffer.limit = 0;
  input->buffer.size = 0;
  /* Initialize the output buffer structure. */
  output->text_mode = text_mode;
  output->object = object;
  output->write = write;
  output->flush = flush;
  output->buffer.unmanaged = 0;
  output->buffer.data = NULL;
  output->buffer.index = 0;
  output->buffer.limit = 0;
  output->buffer.size = 0;
}

/* init_unmanaged_buffers() {{{1  */

void init_unmanaged_buffers(lua_State *L,
    lua_apr_readbuf *input, lua_apr_writebuf *output,
    char *data, size_t size)
{
  /* Initialize the input buffer structure. */
  input->text_mode = 0;
  input->object = NULL;
  input->read = NULL;
  input->buffer.unmanaged = 1;
  input->buffer.data = data;
  input->buffer.index = 0;
  input->buffer.limit = size;
  input->buffer.size = size;
  /* Initialize the output buffer structure. */
  output->text_mode = 0;
  output->object = NULL;
  output->write = NULL;
  output->flush = NULL;
  output->buffer.unmanaged = 1;
  output->buffer.data = data;
  output->buffer.index = 0;
  output->buffer.limit = 0;
  output->buffer.size = size;
}

/* free_buffer() {{{1 */

void free_buffer(lua_State *L, lua_apr_buffer *B)
{
  if (!B->unmanaged && B->data != NULL) {
    free(B->data);
    B->data = NULL;
    B->index = 0;
    B->limit = 0;
    B->size = 0;
  }
}

/* read_lines() {{{1 */

int read_lines(lua_State *L, lua_apr_readbuf *B)
{
  /* Return the iterator function. */
  lua_pushlightuserdata(L, B);
  lua_pushcclosure(L, read_lines_cb, 1);
  return 1;
}

/* read_buffer() {{{1 */

int read_buffer(lua_State *L, lua_apr_readbuf *B)
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

  if (!SUCCESS_OR_EOF(&B->buffer, status)) {
    /* Replace results with (nil, message, code). */
    lua_settop(L, 1);
    nresults = push_error_status(L, status);
  }

  return nresults;
}

/* write_buffer() {{{1 */

int write_buffer(lua_State *L, lua_apr_writebuf *output)
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

/* flush_buffer() {{{1 */

apr_status_t flush_buffer(lua_State *L, lua_apr_writebuf *output, int soft)
{
  lua_apr_buffer *B = &output->buffer;
  apr_status_t status = APR_SUCCESS;
  apr_size_t len;

  /* Don't do anything for unmanaged buffers. */
  if (output->buffer.unmanaged)
    return status;

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
