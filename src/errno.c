/* Error handling module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: May 15, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * Most functions in the Lua/APR binding follow the Lua idiom of returning nil
 * followed by an error message string. These functions also return a third
 * argument which is the symbolic name of the error (or the error code in case
 * a symbolic name cannot be determined). The following symbolic names are
 * currently defined (there's actually a lot more but they shouldn't be
 * relevant when working in Lua):
 *
 *  - `'ENOSTAT'`: APR was unable to perform a stat on the file
 *  - `'EBADDATE'`: APR was given an invalid date
 *  - `'EINVALSOCK'`: APR was given an invalid socket
 *  - `'ENOPROC'`: APR was not given a process structure
 *  - `'ENOTIME'`: APR was not given a time structure
 *  - `'ENODIR'`: APR was not given a directory structure
 *  - `'ENOTHREAD'`: APR was not given a thread structure
 *  - `'EBADIP'`: the specified IP address is invalid
 *  - `'EBADMASK'`: the specified netmask is invalid
 *  - `'EDSOOPEN'`: APR was unable to open the DSO object
 *  - `'EABSOLUTE'`: the given path was absolute
 *  - `'ERELATIVE'`: the given path was relative
 *  - `'EINCOMPLETE'`: the given path was neither relative nor absolute
 *  - `'EABOVEROOT'`: the given path was above the root path
 *  - `'EBADPATH'`: the given path was bad
 *  - `'EPATHWILD'`: the given path contained wildcards
 *  - `'ESYMNOTFOUND'`: could not find the requested symbol
 *  - `'EPROC_UNKNOWN'`: the given process was not recognized by APR
 *  - `'ENOTENOUGHENTROPY'`: APR could not gather enough [entropy] [entropy] to continue
 *  - `'TIMEUP'`: the operation did not finish before the timeout
 *  - `'INCOMPLETE'`: the operation was incomplete although some processing was performed and the results are partially valid
 *  - `'EOF'`: APR has encountered the end of the file
 *  - `'ENOTIMPL'`: the APR function has not been implemented on this platform, either because nobody has gotten to it yet, or the function is impossible on this platform
 *  - `'EMISMATCH'`: two passwords do not match
 *  - `'EACCES'`: permission denied
 *  - `'EEXIST'`: file exists
 *  - `'ENAMETOOLONG'`: path name is too long
 *  - `'ENOENT'`: no such file or directory
 *  - `'ENOTDIR'`: not a directory
 *  - `'ENOSPC'`: no space left on device
 *  - `'ENOMEM'`: not enough memory
 *  - `'EMFILE'`: too many open files
 *  - `'ENFILE'`: file table overflow
 *  - `'EBADF'`: bad file number
 *  - `'EINVAL'`: invalid argument
 *  - `'ESPIPE'`: illegal seek
 *  - `'EAGAIN'`: operation would block
 *  - `'EINTR'`: interrupted system call
 *  - `'ENOTSOCK'`: socket operation on a non-socket
 *  - `'ECONNREFUSED'`: connection refused
 *  - `'EINPROGRESS'`: operation now in progress
 *  - `'ECONNABORTED'`: software caused connection abort
 *  - `'ECONNRESET'`: connection Reset by peer
 *  - `'ETIMEDOUT'`: operation timed out (deprecated)
 *  - `'EHOSTUNREACH'`: no route to host
 *  - `'ENETUNREACH'`: network is unreachable
 *  - `'EFTYPE'`: inappropriate file type or format
 *  - `'EPIPE'`: broken pipe
 *  - `'EXDEV'`: cross device link
 *  - `'ENOTEMPTY'`: directory not empty
 *  - `'EAFNOSUPPORT'`: address family not supported
 *
 * Note that the error descriptions above were copied verbatim from [apr_errno.h] [errno].
 *
 * [entropy]: http://en.wikipedia.org/wiki/Entropy_%28computing%29
 * [errno]: http://svn.apache.org/viewvc/apr/apr/trunk/include/apr_errno.h?view=markup
 */

#include "lua_apr.h"
#include <apr_errno.h>

void status_to_name(lua_State *L, apr_status_t status)
{
  /* Use a switch statement for fast number to string mapping: */
  switch (status) {
    case APR_ANONYMOUS:         lua_pushliteral(L, "ANONYMOUS");         return;
    case APR_BADARG:            lua_pushliteral(L, "BADARG");            return;
    case APR_BADCH:             lua_pushliteral(L, "BADCH");             return;
    case APR_DETACH:            lua_pushliteral(L, "DETACH");            return;
    case APR_EABOVEROOT:        lua_pushliteral(L, "EABOVEROOT");        return;
    case APR_EABSOLUTE:         lua_pushliteral(L, "EABSOLUTE");         return;
    case APR_EACCES:            lua_pushliteral(L, "EACCES");            return;
    case APR_EAFNOSUPPORT:      lua_pushliteral(L, "EAFNOSUPPORT");      return;
    case APR_EAGAIN:            lua_pushliteral(L, "EAGAIN");            return;
    case APR_EBADDATE:          lua_pushliteral(L, "EBADDATE");          return;
    case APR_EBADF:             lua_pushliteral(L, "EBADF");             return;
    case APR_EBADIP:            lua_pushliteral(L, "EBADIP");            return;
    case APR_EBADMASK:          lua_pushliteral(L, "EBADMASK");          return;
    case APR_EBADPATH:          lua_pushliteral(L, "EBADPATH");          return;
    case APR_EBUSY:             lua_pushliteral(L, "EBUSY");             return;
    case APR_ECONNABORTED:      lua_pushliteral(L, "ECONNABORTED");      return;
    case APR_ECONNREFUSED:      lua_pushliteral(L, "ECONNREFUSED");      return;
    case APR_ECONNRESET:        lua_pushliteral(L, "ECONNRESET");        return;
    case APR_EDSOOPEN:          lua_pushliteral(L, "EDSOOPEN");          return;
    case APR_EEXIST:            lua_pushliteral(L, "EEXIST");            return;
    case APR_EFTYPE:            lua_pushliteral(L, "EFTYPE");            return;
    case APR_EGENERAL:          lua_pushliteral(L, "EGENERAL");          return;
    case APR_EHOSTUNREACH:      lua_pushliteral(L, "EHOSTUNREACH");      return;
    case APR_EINCOMPLETE:       lua_pushliteral(L, "EINCOMPLETE");       return;
    case APR_EINIT:             lua_pushliteral(L, "EINIT");             return;
    case APR_EINPROGRESS:       lua_pushliteral(L, "EINPROGRESS");       return;
    case APR_EINTR:             lua_pushliteral(L, "EINTR");             return;
    case APR_EINVAL:            lua_pushliteral(L, "EINVAL");            return;
    case APR_EINVALSOCK:        lua_pushliteral(L, "EINVALSOCK");        return;
    case APR_EMFILE:            lua_pushliteral(L, "EMFILE");            return;
    case APR_EMISMATCH:         lua_pushliteral(L, "EMISMATCH");         return;
    case APR_ENAMETOOLONG:      lua_pushliteral(L, "ENAMETOOLONG");      return;
    case APR_ENETUNREACH:       lua_pushliteral(L, "ENETUNREACH");       return;
    case APR_ENFILE:            lua_pushliteral(L, "ENFILE");            return;
    case APR_ENODIR:            lua_pushliteral(L, "ENODIR");            return;
    case APR_ENOENT:            lua_pushliteral(L, "ENOENT");            return;
    case APR_ENOLOCK:           lua_pushliteral(L, "ENOLOCK");           return;
    case APR_ENOMEM:            lua_pushliteral(L, "ENOMEM");            return;
    case APR_ENOPOLL:           lua_pushliteral(L, "ENOPOLL");           return;
    case APR_ENOPOOL:           lua_pushliteral(L, "ENOPOOL");           return;
    case APR_ENOPROC:           lua_pushliteral(L, "ENOPROC");           return;
    case APR_ENOSHMAVAIL:       lua_pushliteral(L, "ENOSHMAVAIL");       return;
    case APR_ENOSOCKET:         lua_pushliteral(L, "ENOSOCKET");         return;
    case APR_ENOSPC:            lua_pushliteral(L, "ENOSPC");            return;
    case APR_ENOSTAT:           lua_pushliteral(L, "ENOSTAT");           return;
    case APR_ENOTDIR:           lua_pushliteral(L, "ENOTDIR");           return;
    case APR_ENOTEMPTY:         lua_pushliteral(L, "ENOTEMPTY");         return;
    case APR_ENOTENOUGHENTROPY: lua_pushliteral(L, "ENOTENOUGHENTROPY"); return;
    case APR_ENOTHDKEY:         lua_pushliteral(L, "ENOTHDKEY");         return;
    case APR_ENOTHREAD:         lua_pushliteral(L, "ENOTHREAD");         return;
    case APR_ENOTIME:           lua_pushliteral(L, "ENOTIME");           return;
    case APR_ENOTIMPL:          lua_pushliteral(L, "ENOTIMPL");          return;
    case APR_ENOTSOCK:          lua_pushliteral(L, "ENOTSOCK");          return;
    case APR_EOF:               lua_pushliteral(L, "EOF");               return;
    case APR_EPATHWILD:         lua_pushliteral(L, "EPATHWILD");         return;
    case APR_EPIPE:             lua_pushliteral(L, "EPIPE");             return;
    case APR_EPROC_UNKNOWN:     lua_pushliteral(L, "EPROC_UNKNOWN");     return;
    case APR_ERELATIVE:         lua_pushliteral(L, "ERELATIVE");         return;
    case APR_ESPIPE:            lua_pushliteral(L, "ESPIPE");            return;
    case APR_ESYMNOTFOUND:      lua_pushliteral(L, "ESYMNOTFOUND");      return;
    case APR_ETIMEDOUT:         lua_pushliteral(L, "ETIMEDOUT");         return;
    case APR_EXDEV:             lua_pushliteral(L, "EXDEV");             return;
    case APR_FILEBASED:         lua_pushliteral(L, "FILEBASED");         return;
    case APR_INCHILD:           lua_pushliteral(L, "INCHILD");           return;
    case APR_INCOMPLETE:        lua_pushliteral(L, "INCOMPLETE");        return;
    case APR_INPARENT:          lua_pushliteral(L, "INPARENT");          return;
    case APR_KEYBASED:          lua_pushliteral(L, "KEYBASED");          return;
    case APR_NOTDETACH:         lua_pushliteral(L, "NOTDETACH");         return;
    case APR_NOTFOUND:          lua_pushliteral(L, "NOTFOUND");          return;
    case APR_SUCCESS:           lua_pushliteral(L, "SUCCESS");           return;
    case APR_TIMEUP:            lua_pushliteral(L, "TIMEUP");            return;
  }

  /* If the switch statement fails we fall back to the following monstrosity :-) */
  if (0) ;
  else if (APR_STATUS_IS_ANONYMOUS(status))         { lua_pushliteral(L, "ANONYMOUS");         return; }
  else if (APR_STATUS_IS_BADARG(status))            { lua_pushliteral(L, "BADARG");            return; }
  else if (APR_STATUS_IS_BADCH(status))             { lua_pushliteral(L, "BADCH");             return; }
  else if (APR_STATUS_IS_DETACH(status))            { lua_pushliteral(L, "DETACH");            return; }
  else if (APR_STATUS_IS_EABOVEROOT(status))        { lua_pushliteral(L, "EABOVEROOT");        return; }
  else if (APR_STATUS_IS_EABSOLUTE(status))         { lua_pushliteral(L, "EABSOLUTE");         return; }
  else if (APR_STATUS_IS_EACCES(status))            { lua_pushliteral(L, "EACCES");            return; }
  else if (APR_STATUS_IS_EAFNOSUPPORT(status))      { lua_pushliteral(L, "EAFNOSUPPORT");      return; }
  else if (APR_STATUS_IS_EAGAIN(status))            { lua_pushliteral(L, "EAGAIN");            return; }
  else if (APR_STATUS_IS_EBADDATE(status))          { lua_pushliteral(L, "EBADDATE");          return; }
  else if (APR_STATUS_IS_EBADF(status))             { lua_pushliteral(L, "EBADF");             return; }
  else if (APR_STATUS_IS_EBADIP(status))            { lua_pushliteral(L, "EBADIP");            return; }
  else if (APR_STATUS_IS_EBADMASK(status))          { lua_pushliteral(L, "EBADMASK");          return; }
  else if (APR_STATUS_IS_EBADPATH(status))          { lua_pushliteral(L, "EBADPATH");          return; }
  else if (APR_STATUS_IS_EBUSY(status))             { lua_pushliteral(L, "EBUSY");             return; }
  else if (APR_STATUS_IS_ECONNABORTED(status))      { lua_pushliteral(L, "ECONNABORTED");      return; }
  else if (APR_STATUS_IS_ECONNREFUSED(status))      { lua_pushliteral(L, "ECONNREFUSED");      return; }
  else if (APR_STATUS_IS_ECONNRESET(status))        { lua_pushliteral(L, "ECONNRESET");        return; }
  else if (APR_STATUS_IS_EDSOOPEN(status))          { lua_pushliteral(L, "EDSOOPEN");          return; }
  else if (APR_STATUS_IS_EEXIST(status))            { lua_pushliteral(L, "EEXIST");            return; }
  else if (APR_STATUS_IS_EFTYPE(status))            { lua_pushliteral(L, "EFTYPE");            return; }
  else if (APR_STATUS_IS_EGENERAL(status))          { lua_pushliteral(L, "EGENERAL");          return; }
  else if (APR_STATUS_IS_EHOSTUNREACH(status))      { lua_pushliteral(L, "EHOSTUNREACH");      return; }
  else if (APR_STATUS_IS_EINCOMPLETE(status))       { lua_pushliteral(L, "EINCOMPLETE");       return; }
  else if (APR_STATUS_IS_EINIT(status))             { lua_pushliteral(L, "EINIT");             return; }
  else if (APR_STATUS_IS_EINPROGRESS(status))       { lua_pushliteral(L, "EINPROGRESS");       return; }
  else if (APR_STATUS_IS_EINTR(status))             { lua_pushliteral(L, "EINTR");             return; }
  else if (APR_STATUS_IS_EINVAL(status))            { lua_pushliteral(L, "EINVAL");            return; }
  else if (APR_STATUS_IS_EINVALSOCK(status))        { lua_pushliteral(L, "EINVALSOCK");        return; }
  else if (APR_STATUS_IS_EMFILE(status))            { lua_pushliteral(L, "EMFILE");            return; }
  else if (APR_STATUS_IS_EMISMATCH(status))         { lua_pushliteral(L, "EMISMATCH");         return; }
  else if (APR_STATUS_IS_ENAMETOOLONG(status))      { lua_pushliteral(L, "ENAMETOOLONG");      return; }
  else if (APR_STATUS_IS_ENETUNREACH(status))       { lua_pushliteral(L, "ENETUNREACH");       return; }
  else if (APR_STATUS_IS_ENFILE(status))            { lua_pushliteral(L, "ENFILE");            return; }
  else if (APR_STATUS_IS_ENODIR(status))            { lua_pushliteral(L, "ENODIR");            return; }
  else if (APR_STATUS_IS_ENOENT(status))            { lua_pushliteral(L, "ENOENT");            return; }
  else if (APR_STATUS_IS_ENOLOCK(status))           { lua_pushliteral(L, "ENOLOCK");           return; }
  else if (APR_STATUS_IS_ENOMEM(status))            { lua_pushliteral(L, "ENOMEM");            return; }
  else if (APR_STATUS_IS_ENOPOLL(status))           { lua_pushliteral(L, "ENOPOLL");           return; }
  else if (APR_STATUS_IS_ENOPOOL(status))           { lua_pushliteral(L, "ENOPOOL");           return; }
  else if (APR_STATUS_IS_ENOPROC(status))           { lua_pushliteral(L, "ENOPROC");           return; }
  else if (APR_STATUS_IS_ENOSHMAVAIL(status))       { lua_pushliteral(L, "ENOSHMAVAIL");       return; }
  else if (APR_STATUS_IS_ENOSOCKET(status))         { lua_pushliteral(L, "ENOSOCKET");         return; }
  else if (APR_STATUS_IS_ENOSPC(status))            { lua_pushliteral(L, "ENOSPC");            return; }
  else if (APR_STATUS_IS_ENOSTAT(status))           { lua_pushliteral(L, "ENOSTAT");           return; }
  else if (APR_STATUS_IS_ENOTDIR(status))           { lua_pushliteral(L, "ENOTDIR");           return; }
  else if (APR_STATUS_IS_ENOTEMPTY(status))         { lua_pushliteral(L, "ENOTEMPTY");         return; }
  else if (APR_STATUS_IS_ENOTENOUGHENTROPY(status)) { lua_pushliteral(L, "ENOTENOUGHENTROPY"); return; }
  else if (APR_STATUS_IS_ENOTHDKEY(status))         { lua_pushliteral(L, "ENOTHDKEY");         return; }
  else if (APR_STATUS_IS_ENOTHREAD(status))         { lua_pushliteral(L, "ENOTHREAD");         return; }
  else if (APR_STATUS_IS_ENOTIME(status))           { lua_pushliteral(L, "ENOTIME");           return; }
  else if (APR_STATUS_IS_ENOTIMPL(status))          { lua_pushliteral(L, "ENOTIMPL");          return; }
  else if (APR_STATUS_IS_ENOTSOCK(status))          { lua_pushliteral(L, "ENOTSOCK");          return; }
  else if (APR_STATUS_IS_EOF(status))               { lua_pushliteral(L, "EOF");               return; }
  else if (APR_STATUS_IS_EPATHWILD(status))         { lua_pushliteral(L, "EPATHWILD");         return; }
  else if (APR_STATUS_IS_EPIPE(status))             { lua_pushliteral(L, "EPIPE");             return; }
  else if (APR_STATUS_IS_EPROC_UNKNOWN(status))     { lua_pushliteral(L, "EPROC_UNKNOWN");     return; }
  else if (APR_STATUS_IS_ERELATIVE(status))         { lua_pushliteral(L, "ERELATIVE");         return; }
  else if (APR_STATUS_IS_ESPIPE(status))            { lua_pushliteral(L, "ESPIPE");            return; }
  else if (APR_STATUS_IS_ESYMNOTFOUND(status))      { lua_pushliteral(L, "ESYMNOTFOUND");      return; }
  else if (APR_STATUS_IS_ETIMEDOUT(status))         { lua_pushliteral(L, "ETIMEDOUT");         return; }
  else if (APR_STATUS_IS_EXDEV(status))             { lua_pushliteral(L, "EXDEV");             return; }
  else if (APR_STATUS_IS_FILEBASED(status))         { lua_pushliteral(L, "FILEBASED");         return; }
  else if (APR_STATUS_IS_INCHILD(status))           { lua_pushliteral(L, "INCHILD");           return; }
  else if (APR_STATUS_IS_INCOMPLETE(status))        { lua_pushliteral(L, "INCOMPLETE");        return; }
  else if (APR_STATUS_IS_INPARENT(status))          { lua_pushliteral(L, "INPARENT");          return; }
  else if (APR_STATUS_IS_KEYBASED(status))          { lua_pushliteral(L, "KEYBASED");          return; }
  else if (APR_STATUS_IS_NOTDETACH(status))         { lua_pushliteral(L, "NOTDETACH");         return; }
  else if (APR_STATUS_IS_NOTFOUND(status))          { lua_pushliteral(L, "NOTFOUND");          return; }
  else if (APR_STATUS_IS_TIMEUP(status))            { lua_pushliteral(L, "TIMEUP");            return; }

  /* This might be a bug in the script that generated this source code? */
  LUA_APR_DBG("Lua/APR status_to_name(%i) failed, might be a bug?", status);
  lua_pushinteger(L, status);
}
