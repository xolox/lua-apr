local verbose = false
local constants = {}
local tests = {}
local ignored = {
  APR_AFNOSUPPORT = true, -- error: ‘APR_AFNOSUPPORT’ undeclared
  APR_CHILD_DONE = true,
  APR_CHILD_NOTDONE = true,
  APR_DECLARE = true,
  APR_ERRNO_H = true,
  APR_FROM_OS_ERROR = true,
  APR_HAVE_ERRNO_H = true,
  APR_OS2_STATUS = true,
  APR_OS_ERRSPACE_SIZE = true,
  APR_OS_START_CANONERR = true,
  APR_OS_START_EAIERR = true,
  APR_OS_START_ERROR = true,
  APR_OS_START_STATUS = true,
  APR_OS_START_SYSERR = true,
  APR_OS_START_USEERR = true,
  APR_OS_START_USERERR = true,
  APR_OS_START_USERERR = true,
  APR_STATUS_IS_CHILD_DONE = true,
  APR_STATUS_IS_CHILD_NOTDONE = true,
  APR_STATUS_IS_ENETDOWN = true, -- warning: implicit declaration of function ‘APR_STATUS_IS_ENETDOWN’
  APR_TO_OS_ERROR = true,
  APR_UTIL_ERRSPACE_SIZE = true,
  APR_UTIL_START_STATUS = true,
}

local function add(table, token)
  if not table[token] then
    table[#table + 1] = token
    table[token] = true
  end
end

assert(io.input '/usr/include/apr-1.0/apr_errno.h')
local source = io.read '*a'
source = source:gsub('/%*.-%*/', '')
local longest_constant, longest_test = 0, 0
for token in source :gmatch '[%w_]+' do
  if not ignored[token] then
    if token:find '^APR_STATUS_IS_[A-Z0-9_]+$' then
      add(tests, token)
      longest_test = math.max(longest_test, #token)
    elseif token:find '^APR_[A-Z0-9_]+$' then
      add(constants, token)
      longest_constant = math.max(longest_constant, #token)
    end
  end
end

table.sort(constants)
table.sort(tests)

if verbose then

  io.write('Found ', #constants, ' error constants:\n')
  for i, constant in ipairs(constants) do
    io.write(' - ', constant, '\n')
    if not tests[constant:gsub('^APR_(.-)$', 'APR_STATUS_IS_%1')] then
      io.write("   (doesn't have a matching test)\n")
    end
    longest_constant = math.max(#constant, longest_constant)
  end
  io.write '\n'

  io.write('Found ', #tests, ' error tests:\n')
  for i, test in ipairs(tests) do
    io.write(' - ', test, '\n')
    if not constants[test:gsub('^APR_STATUS_IS_', 'APR_')] then
      io.write("   (doesn't have a matching constant)\n")
    end
    longest_test = math.max(#test, longest_test)
  end
  io.write '\n'

end

io.write([[
/* Error handling module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: ]], os.date '%B %d, %Y\n', [[
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
 *  - `'ENOTENOUGHENTROPY'`: APR could not gather enough [entropy] [wp:entropy] to continue
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
 * [wp:entropy]: http://en.wikipedia.org/wiki/Entropy_%28computing%29
 */

#include "lua_apr.h"
#include <apr_errno.h>

void status_to_name(lua_State *L, apr_status_t status)
{
  /* Use a switch statement for fast number to string mapping: */
  switch (status) {
]])

local template = '    case %s: %slua_pushliteral(L, %q); %sreturn;\n'
for i, constant in ipairs(constants) do
  local name = constant:gsub('^APR_', '')
  local padding = string.rep(' ', longest_constant - #constant)
  io.write(template:format(constant, padding, name, padding))
end

io.write [[
  }

  /* If the switch statement fails we fall back to the following monstrosity :-) */
  if (0) ;
]]

local template = '  else if (%s(status)) %s{ lua_pushliteral(L, %q); %sreturn; }\n'
for i, test in ipairs(tests) do
  local name = test:gsub('^APR_STATUS_IS_', '')
  local padding = string.rep(' ', longest_test - #test)
  io.write(template:format(test, padding, name, padding))
end

io.write [[

  /* This might be a bug in the script that generated this source code? */
  LUA_APR_DBG("Lua/APR status_to_name(%i) failed, might be a bug?", status);
  lua_pushinteger(L, status);
}
]]
