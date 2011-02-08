/* File system permissions module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: December 28, 2010
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * The Apache Portable Runtime represents file system permissions somewhat
 * similar to those of [UNIX] [unix]. There are three categories of
 * permissions: the user, the group and everyone else (the world). Each
 * category supports read and write permission bits while the meaning of the
 * third permission bit differs between categories.
 *
 * [unix]: http://en.wikipedia.org/wiki/Unix
 *
 * ### How Lua/APR presents permissions
 *
 * The Lua/APR binding uses a string of 9 characters to represent file system
 * permissions such as those returned by `apr.stat()`. Here's an example:
 *
 *     > = apr.stat('.', 'protection')
 *     'rwxr-xr-x'
 *
 * This is the syntax of these permissions strings:
 *
 *  - Character 1: `r` if the user has read permissions, `-` otherwise
 *  - Character 2: `w` if the user has write permissions, `-` otherwise
 *  - Character 3: `x` if the user has execute permissions, `S` if the user
 *    has set user id permissions or `s` if the user has both permissions
 *  - Characters 4..6: the same respective permissions for the group
 *  - Characters 7..8: the same respective permissions for the world
 *  - Character 9: `x` if the world has execute permissions, `T` if the world
 *    has sticky permissions or `t` if the world has both permissions
 *
 * As an example, `rwxrwx---` means the user and group have full permissions
 * while the world has none. Another example: `r-xr-xr-x` means no-one has
 * write permissions.
 *
 * ### How you can request permissions
 *
 * When you need to request file system permissions for an operation like
 * [reading](#apr.file_open) or [copying](#apr.file_copy) a file there are two
 * string formats you can use. The first format is a string of nine characters
 * that lists each permission explicitly. This is the format documented above.
 *
 * The second format is very flexible and is one of the formats accepted by the
 * Linux command line program [chmod] [chmod]. The permissions are split in
 * three groups with a one-letter code: user is `u`, group is `g` and world is
 * `o` (for "others"). One or more permissions can then be assigned to one or
 * more of these groups. Here's an example that requests read permission for
 * user, group and others: `ugo=r`. Now when you also need write permission for
 * user and group, you can use `ugo=r,ug=w`.
 *
 * [chmod]: http://en.wikipedia.org/wiki/chmod
 */

#include "lua_apr.h"

/* Character constants used to represent permission bits */

#define NONE        '-'
#define READ        'r'
#define WRITE       'w'
#define EXEC        'x'
#define SETID       'S'
#define STICKY      'T'
#define EXEC_AND(S) (S + 32)

/* Convert an APR bitfield to a Lua string */

int push_protection(lua_State *L, apr_fileperms_t perm)
{
  char str[9], *p = str;

#define UNPARSE(KIND, CHAR, MAGIC) \
    *p++ = (perm & APR_FPROT_ ## KIND ## READ) ? 'r' : '-'; \
    *p++ = (perm & APR_FPROT_ ## KIND ## WRITE) ? 'w' : '-'; \
    if ((perm & APR_FPROT_ ## KIND ## EXECUTE) && \
        (perm & APR_FPROT_ ## KIND ## MAGIC)) \
      *p++ = CHAR; \
    else if (perm & APR_FPROT_ ## KIND ## MAGIC) \
      *p++ = (CHAR - 32); \
    else if (perm & APR_FPROT_ ## KIND ## EXECUTE) \
      *p++ = 'x'; \
    else \
      *p++ = '-'

  UNPARSE(U, 's', SETID);
  UNPARSE(G, 's', SETID);
  UNPARSE(W, 't', STICKY);

#undef UNPARSE

  lua_pushlstring(L, str, sizeof str);
  return 1;
}

/* Convert a Lua value to an APR bitfield of filesystem permission flags */

apr_fileperms_t check_permissions(lua_State *L, int idx, int inherit)
{
  enum { USER = 0x01, GROUP = 0x02, OTHER = 0x04 } whom;
  apr_fileperms_t output = 0;
  char *input;
  int loop;

  if (!lua_isstring(L, idx))
    return (inherit && lua_toboolean(L, idx))
      ? APR_FPROT_FILE_SOURCE_PERMS
      : APR_FPROT_OS_DEFAULT;

  /* Note: Though the "input" pointer is incremented, the Lua string
   * isn't modified. I just don't know to express this in C casts... */
  input = (char *) lua_tostring(L, idx);

# define MATCH(offset, special) ( \
    (input[offset+0] == NONE || input[offset+0] == READ) && \
    (input[offset+1] == NONE || input[offset+1] == WRITE) && \
    (input[offset+2] == NONE || input[offset+2] == EXEC || \
    input[offset+2] == special || input[offset+2] == EXEC_AND(special)))

  if (MATCH(0, SETID) && MATCH(3, SETID) && MATCH(6, STICKY)) {

#undef MATCH

# define PARSE(offset, special, class) \
    if (input[offset+0] == READ) \
      output |= APR_FPROT_ ## class ## READ; \
    if (input[offset+1] == WRITE) \
      output |= APR_FPROT_ ## class ## WRITE; \
    if (input[offset+2] == EXEC_AND(special)) \
      output |= APR_FPROT_ ## class ## EXECUTE | APR_FPROT_ ## class ## special; \
    else if (input[offset+2] == special) \
      output |= APR_FPROT_ ## class ## special; \
    else if (input[offset+2] == EXEC) \
      output |= APR_FPROT_ ## class ## EXECUTE

    PARSE(0, SETID, U);
    PARSE(3, SETID, G);
    PARSE(6, STICKY, W);

#undef PARSE

    return output;
  }

  for (whom = 0;;) {
    switch (*input++) {
      case '\0':
        /* Lua API always \0 terminates. */
        return output;
      case 'u':
        whom |= USER;
        break;
      case 'g':
        whom |= GROUP;
        break;
      case 'o':
        whom |= OTHER;
        break;
      default:
        whom |= USER | GROUP | OTHER;
        input--;
        /* fall through! */
      case '=':
        for (loop = 1; loop;) {
          switch (*input++) {
            default:
              return output;
            case ',':
              loop = 0;
              whom = 0;
              break;
            case READ:
              if (whom & USER) output |= APR_FPROT_UREAD;
              if (whom & GROUP) output |= APR_FPROT_GREAD;
              if (whom & OTHER) output |= APR_FPROT_WREAD;
              break;
            case WRITE:
              if (whom & USER) output |= APR_FPROT_UWRITE;
              if (whom & GROUP) output |= APR_FPROT_GWRITE;
              if (whom & OTHER) output |= APR_FPROT_WWRITE;
              break;
            case EXEC:
              if (whom & USER) output |= APR_FPROT_UEXECUTE;
              if (whom & GROUP) output |= APR_FPROT_GEXECUTE;
              if (whom & OTHER) output |= APR_FPROT_WEXECUTE;
              break;
            case SETID:
              if (whom & USER) output |= APR_FPROT_USETID;
              if (whom & GROUP) output |= APR_FPROT_GSETID;
              break;
            case STICKY:
              if (whom & OTHER) output |= APR_FPROT_WSTICKY;
              break;
          }
        }
    }
  }
}

/* vim: set ts=2 sw=2 et tw=79 fen fdm=marker : */
