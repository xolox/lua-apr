/* Error handling module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: December 07, 2011
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
#   ifdef APR_ANONYMOUS
    case APR_ANONYMOUS:
      lua_pushliteral(L, "ANONYMOUS");
      return;
#   endif
#   ifdef APR_BADARG
    case APR_BADARG:
      lua_pushliteral(L, "BADARG");
      return;
#   endif
#   ifdef APR_BADCH
    case APR_BADCH:
      lua_pushliteral(L, "BADCH");
      return;
#   endif
#   ifdef APR_DETACH
    case APR_DETACH:
      lua_pushliteral(L, "DETACH");
      return;
#   endif
#   ifdef APR_EABOVEROOT
    case APR_EABOVEROOT:
      lua_pushliteral(L, "EABOVEROOT");
      return;
#   endif
#   ifdef APR_EABSOLUTE
    case APR_EABSOLUTE:
      lua_pushliteral(L, "EABSOLUTE");
      return;
#   endif
#   ifdef APR_EACCES
    case APR_EACCES:
      lua_pushliteral(L, "EACCES");
      return;
#   endif
#   ifdef APR_EAFNOSUPPORT
    case APR_EAFNOSUPPORT:
      lua_pushliteral(L, "EAFNOSUPPORT");
      return;
#   endif
#   ifdef APR_EAGAIN
    case APR_EAGAIN:
      lua_pushliteral(L, "EAGAIN");
      return;
#   endif
#   ifdef APR_EBADDATE
    case APR_EBADDATE:
      lua_pushliteral(L, "EBADDATE");
      return;
#   endif
#   ifdef APR_EBADF
    case APR_EBADF:
      lua_pushliteral(L, "EBADF");
      return;
#   endif
#   ifdef APR_EBADIP
    case APR_EBADIP:
      lua_pushliteral(L, "EBADIP");
      return;
#   endif
#   ifdef APR_EBADMASK
    case APR_EBADMASK:
      lua_pushliteral(L, "EBADMASK");
      return;
#   endif
#   ifdef APR_EBADPATH
    case APR_EBADPATH:
      lua_pushliteral(L, "EBADPATH");
      return;
#   endif
#   ifdef APR_EBUSY
    case APR_EBUSY:
      lua_pushliteral(L, "EBUSY");
      return;
#   endif
#   ifdef APR_ECONNABORTED
    case APR_ECONNABORTED:
      lua_pushliteral(L, "ECONNABORTED");
      return;
#   endif
#   ifdef APR_ECONNREFUSED
    case APR_ECONNREFUSED:
      lua_pushliteral(L, "ECONNREFUSED");
      return;
#   endif
#   ifdef APR_ECONNRESET
    case APR_ECONNRESET:
      lua_pushliteral(L, "ECONNRESET");
      return;
#   endif
#   ifdef APR_EDSOOPEN
    case APR_EDSOOPEN:
      lua_pushliteral(L, "EDSOOPEN");
      return;
#   endif
#   ifdef APR_EEXIST
    case APR_EEXIST:
      lua_pushliteral(L, "EEXIST");
      return;
#   endif
#   ifdef APR_EFTYPE
    case APR_EFTYPE:
      lua_pushliteral(L, "EFTYPE");
      return;
#   endif
#   ifdef APR_EGENERAL
    case APR_EGENERAL:
      lua_pushliteral(L, "EGENERAL");
      return;
#   endif
#   ifdef APR_EHOSTUNREACH
    case APR_EHOSTUNREACH:
      lua_pushliteral(L, "EHOSTUNREACH");
      return;
#   endif
#   ifdef APR_EINCOMPLETE
    case APR_EINCOMPLETE:
      lua_pushliteral(L, "EINCOMPLETE");
      return;
#   endif
#   ifdef APR_EINIT
    case APR_EINIT:
      lua_pushliteral(L, "EINIT");
      return;
#   endif
#   ifdef APR_EINPROGRESS
    case APR_EINPROGRESS:
      lua_pushliteral(L, "EINPROGRESS");
      return;
#   endif
#   ifdef APR_EINTR
    case APR_EINTR:
      lua_pushliteral(L, "EINTR");
      return;
#   endif
#   ifdef APR_EINVAL
    case APR_EINVAL:
      lua_pushliteral(L, "EINVAL");
      return;
#   endif
#   ifdef APR_EINVALSOCK
    case APR_EINVALSOCK:
      lua_pushliteral(L, "EINVALSOCK");
      return;
#   endif
#   ifdef APR_EMFILE
    case APR_EMFILE:
      lua_pushliteral(L, "EMFILE");
      return;
#   endif
#   ifdef APR_EMISMATCH
    case APR_EMISMATCH:
      lua_pushliteral(L, "EMISMATCH");
      return;
#   endif
#   ifdef APR_ENAMETOOLONG
    case APR_ENAMETOOLONG:
      lua_pushliteral(L, "ENAMETOOLONG");
      return;
#   endif
#   ifdef APR_ENETUNREACH
    case APR_ENETUNREACH:
      lua_pushliteral(L, "ENETUNREACH");
      return;
#   endif
#   ifdef APR_ENFILE
    case APR_ENFILE:
      lua_pushliteral(L, "ENFILE");
      return;
#   endif
#   ifdef APR_ENODIR
    case APR_ENODIR:
      lua_pushliteral(L, "ENODIR");
      return;
#   endif
#   ifdef APR_ENOENT
    case APR_ENOENT:
      lua_pushliteral(L, "ENOENT");
      return;
#   endif
#   ifdef APR_ENOLOCK
    case APR_ENOLOCK:
      lua_pushliteral(L, "ENOLOCK");
      return;
#   endif
#   ifdef APR_ENOMEM
    case APR_ENOMEM:
      lua_pushliteral(L, "ENOMEM");
      return;
#   endif
#   ifdef APR_ENOPOLL
    case APR_ENOPOLL:
      lua_pushliteral(L, "ENOPOLL");
      return;
#   endif
#   ifdef APR_ENOPOOL
    case APR_ENOPOOL:
      lua_pushliteral(L, "ENOPOOL");
      return;
#   endif
#   ifdef APR_ENOPROC
    case APR_ENOPROC:
      lua_pushliteral(L, "ENOPROC");
      return;
#   endif
#   ifdef APR_ENOSHMAVAIL
    case APR_ENOSHMAVAIL:
      lua_pushliteral(L, "ENOSHMAVAIL");
      return;
#   endif
#   ifdef APR_ENOSOCKET
    case APR_ENOSOCKET:
      lua_pushliteral(L, "ENOSOCKET");
      return;
#   endif
#   ifdef APR_ENOSPC
    case APR_ENOSPC:
      lua_pushliteral(L, "ENOSPC");
      return;
#   endif
#   ifdef APR_ENOSTAT
    case APR_ENOSTAT:
      lua_pushliteral(L, "ENOSTAT");
      return;
#   endif
#   ifdef APR_ENOTDIR
    case APR_ENOTDIR:
      lua_pushliteral(L, "ENOTDIR");
      return;
#   endif
#   ifdef APR_ENOTEMPTY
    case APR_ENOTEMPTY:
      lua_pushliteral(L, "ENOTEMPTY");
      return;
#   endif
#   ifdef APR_ENOTENOUGHENTROPY
    case APR_ENOTENOUGHENTROPY:
      lua_pushliteral(L, "ENOTENOUGHENTROPY");
      return;
#   endif
#   ifdef APR_ENOTHDKEY
    case APR_ENOTHDKEY:
      lua_pushliteral(L, "ENOTHDKEY");
      return;
#   endif
#   ifdef APR_ENOTHREAD
    case APR_ENOTHREAD:
      lua_pushliteral(L, "ENOTHREAD");
      return;
#   endif
#   ifdef APR_ENOTIME
    case APR_ENOTIME:
      lua_pushliteral(L, "ENOTIME");
      return;
#   endif
#   ifdef APR_ENOTIMPL
    case APR_ENOTIMPL:
      lua_pushliteral(L, "ENOTIMPL");
      return;
#   endif
#   ifdef APR_ENOTSOCK
    case APR_ENOTSOCK:
      lua_pushliteral(L, "ENOTSOCK");
      return;
#   endif
#   ifdef APR_EOF
    case APR_EOF:
      lua_pushliteral(L, "EOF");
      return;
#   endif
#   ifdef APR_EPATHWILD
    case APR_EPATHWILD:
      lua_pushliteral(L, "EPATHWILD");
      return;
#   endif
#   ifdef APR_EPIPE
    case APR_EPIPE:
      lua_pushliteral(L, "EPIPE");
      return;
#   endif
#   ifdef APR_EPROC_UNKNOWN
    case APR_EPROC_UNKNOWN:
      lua_pushliteral(L, "EPROC_UNKNOWN");
      return;
#   endif
#   ifdef APR_ERELATIVE
    case APR_ERELATIVE:
      lua_pushliteral(L, "ERELATIVE");
      return;
#   endif
#   ifdef APR_ESPIPE
    case APR_ESPIPE:
      lua_pushliteral(L, "ESPIPE");
      return;
#   endif
#   ifdef APR_ESYMNOTFOUND
    case APR_ESYMNOTFOUND:
      lua_pushliteral(L, "ESYMNOTFOUND");
      return;
#   endif
#   ifdef APR_ETIMEDOUT
    case APR_ETIMEDOUT:
      lua_pushliteral(L, "ETIMEDOUT");
      return;
#   endif
#   ifdef APR_EXDEV
    case APR_EXDEV:
      lua_pushliteral(L, "EXDEV");
      return;
#   endif
#   ifdef APR_FILEBASED
    case APR_FILEBASED:
      lua_pushliteral(L, "FILEBASED");
      return;
#   endif
#   ifdef APR_INCHILD
    case APR_INCHILD:
      lua_pushliteral(L, "INCHILD");
      return;
#   endif
#   ifdef APR_INCOMPLETE
    case APR_INCOMPLETE:
      lua_pushliteral(L, "INCOMPLETE");
      return;
#   endif
#   ifdef APR_INPARENT
    case APR_INPARENT:
      lua_pushliteral(L, "INPARENT");
      return;
#   endif
#   ifdef APR_KEYBASED
    case APR_KEYBASED:
      lua_pushliteral(L, "KEYBASED");
      return;
#   endif
#   ifdef APR_NOTDETACH
    case APR_NOTDETACH:
      lua_pushliteral(L, "NOTDETACH");
      return;
#   endif
#   ifdef APR_NOTFOUND
    case APR_NOTFOUND:
      lua_pushliteral(L, "NOTFOUND");
      return;
#   endif
#   ifdef APR_SUCCESS
    case APR_SUCCESS:
      lua_pushliteral(L, "SUCCESS");
      return;
#   endif
#   ifdef APR_TIMEUP
    case APR_TIMEUP:
      lua_pushliteral(L, "TIMEUP");
      return;
#   endif
  }

  /* If the switch statement fails we fall back to the following monstrosity :-) */
  if (0) ;
# ifdef APR_STATUS_IS_ANONYMOUS
  else if (APR_STATUS_IS_ANONYMOUS(status)) {
    lua_pushliteral(L, "ANONYMOUS");
    return;
  }
# endif
# ifdef APR_STATUS_IS_BADARG
  else if (APR_STATUS_IS_BADARG(status)) {
    lua_pushliteral(L, "BADARG");
    return;
  }
# endif
# ifdef APR_STATUS_IS_BADCH
  else if (APR_STATUS_IS_BADCH(status)) {
    lua_pushliteral(L, "BADCH");
    return;
  }
# endif
# ifdef APR_STATUS_IS_DETACH
  else if (APR_STATUS_IS_DETACH(status)) {
    lua_pushliteral(L, "DETACH");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EABOVEROOT
  else if (APR_STATUS_IS_EABOVEROOT(status)) {
    lua_pushliteral(L, "EABOVEROOT");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EABSOLUTE
  else if (APR_STATUS_IS_EABSOLUTE(status)) {
    lua_pushliteral(L, "EABSOLUTE");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EACCES
  else if (APR_STATUS_IS_EACCES(status)) {
    lua_pushliteral(L, "EACCES");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EAFNOSUPPORT
  else if (APR_STATUS_IS_EAFNOSUPPORT(status)) {
    lua_pushliteral(L, "EAFNOSUPPORT");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EAGAIN
  else if (APR_STATUS_IS_EAGAIN(status)) {
    lua_pushliteral(L, "EAGAIN");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EBADDATE
  else if (APR_STATUS_IS_EBADDATE(status)) {
    lua_pushliteral(L, "EBADDATE");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EBADF
  else if (APR_STATUS_IS_EBADF(status)) {
    lua_pushliteral(L, "EBADF");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EBADIP
  else if (APR_STATUS_IS_EBADIP(status)) {
    lua_pushliteral(L, "EBADIP");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EBADMASK
  else if (APR_STATUS_IS_EBADMASK(status)) {
    lua_pushliteral(L, "EBADMASK");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EBADPATH
  else if (APR_STATUS_IS_EBADPATH(status)) {
    lua_pushliteral(L, "EBADPATH");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EBUSY
  else if (APR_STATUS_IS_EBUSY(status)) {
    lua_pushliteral(L, "EBUSY");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ECONNABORTED
  else if (APR_STATUS_IS_ECONNABORTED(status)) {
    lua_pushliteral(L, "ECONNABORTED");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ECONNREFUSED
  else if (APR_STATUS_IS_ECONNREFUSED(status)) {
    lua_pushliteral(L, "ECONNREFUSED");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ECONNRESET
  else if (APR_STATUS_IS_ECONNRESET(status)) {
    lua_pushliteral(L, "ECONNRESET");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EDSOOPEN
  else if (APR_STATUS_IS_EDSOOPEN(status)) {
    lua_pushliteral(L, "EDSOOPEN");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EEXIST
  else if (APR_STATUS_IS_EEXIST(status)) {
    lua_pushliteral(L, "EEXIST");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EFTYPE
  else if (APR_STATUS_IS_EFTYPE(status)) {
    lua_pushliteral(L, "EFTYPE");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EGENERAL
  else if (APR_STATUS_IS_EGENERAL(status)) {
    lua_pushliteral(L, "EGENERAL");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EHOSTUNREACH
  else if (APR_STATUS_IS_EHOSTUNREACH(status)) {
    lua_pushliteral(L, "EHOSTUNREACH");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EINCOMPLETE
  else if (APR_STATUS_IS_EINCOMPLETE(status)) {
    lua_pushliteral(L, "EINCOMPLETE");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EINIT
  else if (APR_STATUS_IS_EINIT(status)) {
    lua_pushliteral(L, "EINIT");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EINPROGRESS
  else if (APR_STATUS_IS_EINPROGRESS(status)) {
    lua_pushliteral(L, "EINPROGRESS");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EINTR
  else if (APR_STATUS_IS_EINTR(status)) {
    lua_pushliteral(L, "EINTR");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EINVAL
  else if (APR_STATUS_IS_EINVAL(status)) {
    lua_pushliteral(L, "EINVAL");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EINVALSOCK
  else if (APR_STATUS_IS_EINVALSOCK(status)) {
    lua_pushliteral(L, "EINVALSOCK");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EMFILE
  else if (APR_STATUS_IS_EMFILE(status)) {
    lua_pushliteral(L, "EMFILE");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EMISMATCH
  else if (APR_STATUS_IS_EMISMATCH(status)) {
    lua_pushliteral(L, "EMISMATCH");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ENAMETOOLONG
  else if (APR_STATUS_IS_ENAMETOOLONG(status)) {
    lua_pushliteral(L, "ENAMETOOLONG");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ENETUNREACH
  else if (APR_STATUS_IS_ENETUNREACH(status)) {
    lua_pushliteral(L, "ENETUNREACH");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ENFILE
  else if (APR_STATUS_IS_ENFILE(status)) {
    lua_pushliteral(L, "ENFILE");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ENODIR
  else if (APR_STATUS_IS_ENODIR(status)) {
    lua_pushliteral(L, "ENODIR");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ENOENT
  else if (APR_STATUS_IS_ENOENT(status)) {
    lua_pushliteral(L, "ENOENT");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ENOLOCK
  else if (APR_STATUS_IS_ENOLOCK(status)) {
    lua_pushliteral(L, "ENOLOCK");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ENOMEM
  else if (APR_STATUS_IS_ENOMEM(status)) {
    lua_pushliteral(L, "ENOMEM");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ENOPOLL
  else if (APR_STATUS_IS_ENOPOLL(status)) {
    lua_pushliteral(L, "ENOPOLL");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ENOPOOL
  else if (APR_STATUS_IS_ENOPOOL(status)) {
    lua_pushliteral(L, "ENOPOOL");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ENOPROC
  else if (APR_STATUS_IS_ENOPROC(status)) {
    lua_pushliteral(L, "ENOPROC");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ENOSHMAVAIL
  else if (APR_STATUS_IS_ENOSHMAVAIL(status)) {
    lua_pushliteral(L, "ENOSHMAVAIL");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ENOSOCKET
  else if (APR_STATUS_IS_ENOSOCKET(status)) {
    lua_pushliteral(L, "ENOSOCKET");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ENOSPC
  else if (APR_STATUS_IS_ENOSPC(status)) {
    lua_pushliteral(L, "ENOSPC");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ENOSTAT
  else if (APR_STATUS_IS_ENOSTAT(status)) {
    lua_pushliteral(L, "ENOSTAT");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ENOTDIR
  else if (APR_STATUS_IS_ENOTDIR(status)) {
    lua_pushliteral(L, "ENOTDIR");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ENOTEMPTY
  else if (APR_STATUS_IS_ENOTEMPTY(status)) {
    lua_pushliteral(L, "ENOTEMPTY");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ENOTENOUGHENTROPY
  else if (APR_STATUS_IS_ENOTENOUGHENTROPY(status)) {
    lua_pushliteral(L, "ENOTENOUGHENTROPY");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ENOTHDKEY
  else if (APR_STATUS_IS_ENOTHDKEY(status)) {
    lua_pushliteral(L, "ENOTHDKEY");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ENOTHREAD
  else if (APR_STATUS_IS_ENOTHREAD(status)) {
    lua_pushliteral(L, "ENOTHREAD");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ENOTIME
  else if (APR_STATUS_IS_ENOTIME(status)) {
    lua_pushliteral(L, "ENOTIME");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ENOTIMPL
  else if (APR_STATUS_IS_ENOTIMPL(status)) {
    lua_pushliteral(L, "ENOTIMPL");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ENOTSOCK
  else if (APR_STATUS_IS_ENOTSOCK(status)) {
    lua_pushliteral(L, "ENOTSOCK");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EOF
  else if (APR_STATUS_IS_EOF(status)) {
    lua_pushliteral(L, "EOF");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EPATHWILD
  else if (APR_STATUS_IS_EPATHWILD(status)) {
    lua_pushliteral(L, "EPATHWILD");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EPIPE
  else if (APR_STATUS_IS_EPIPE(status)) {
    lua_pushliteral(L, "EPIPE");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EPROC_UNKNOWN
  else if (APR_STATUS_IS_EPROC_UNKNOWN(status)) {
    lua_pushliteral(L, "EPROC_UNKNOWN");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ERELATIVE
  else if (APR_STATUS_IS_ERELATIVE(status)) {
    lua_pushliteral(L, "ERELATIVE");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ESPIPE
  else if (APR_STATUS_IS_ESPIPE(status)) {
    lua_pushliteral(L, "ESPIPE");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ESYMNOTFOUND
  else if (APR_STATUS_IS_ESYMNOTFOUND(status)) {
    lua_pushliteral(L, "ESYMNOTFOUND");
    return;
  }
# endif
# ifdef APR_STATUS_IS_ETIMEDOUT
  else if (APR_STATUS_IS_ETIMEDOUT(status)) {
    lua_pushliteral(L, "ETIMEDOUT");
    return;
  }
# endif
# ifdef APR_STATUS_IS_EXDEV
  else if (APR_STATUS_IS_EXDEV(status)) {
    lua_pushliteral(L, "EXDEV");
    return;
  }
# endif
# ifdef APR_STATUS_IS_FILEBASED
  else if (APR_STATUS_IS_FILEBASED(status)) {
    lua_pushliteral(L, "FILEBASED");
    return;
  }
# endif
# ifdef APR_STATUS_IS_INCHILD
  else if (APR_STATUS_IS_INCHILD(status)) {
    lua_pushliteral(L, "INCHILD");
    return;
  }
# endif
# ifdef APR_STATUS_IS_INCOMPLETE
  else if (APR_STATUS_IS_INCOMPLETE(status)) {
    lua_pushliteral(L, "INCOMPLETE");
    return;
  }
# endif
# ifdef APR_STATUS_IS_INPARENT
  else if (APR_STATUS_IS_INPARENT(status)) {
    lua_pushliteral(L, "INPARENT");
    return;
  }
# endif
# ifdef APR_STATUS_IS_KEYBASED
  else if (APR_STATUS_IS_KEYBASED(status)) {
    lua_pushliteral(L, "KEYBASED");
    return;
  }
# endif
# ifdef APR_STATUS_IS_NOTDETACH
  else if (APR_STATUS_IS_NOTDETACH(status)) {
    lua_pushliteral(L, "NOTDETACH");
    return;
  }
# endif
# ifdef APR_STATUS_IS_NOTFOUND
  else if (APR_STATUS_IS_NOTFOUND(status)) {
    lua_pushliteral(L, "NOTFOUND");
    return;
  }
# endif
# ifdef APR_STATUS_IS_TIMEUP
  else if (APR_STATUS_IS_TIMEUP(status)) {
    lua_pushliteral(L, "TIMEUP");
    return;
  }
# endif

  lua_pushinteger(L, status);
}
