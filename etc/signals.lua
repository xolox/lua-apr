--[[

 Signal handling module code generator for the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: June 23, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

 APR supports signal handling but leaves the definition of signal names up to
 the system where APR is installed. This isn't a problem in C where the usual
 SIG* symbols are available by including <signal.h> but of course that doesn't
 work in Lua! This script works around the problem by generating some really
 nasty preprocessor heavy code :-). The list of signal names in this script was
 taken from "man 7 signal" on my Ubuntu Linux 10.04 installation.

--]]

-- Symbol names.
local symbols = [[
  -- The signals described in the original POSIX.1-1990 standard.
  SIGHUP
  SIGINT
  SIGQUIT
  SIGILL
  SIGABRT
  SIGFPE
  SIGKILL
  SIGSEGV
  SIGPIPE
  SIGALRM
  SIGTERM
  SIGUSR1
  SIGUSR2
  SIGCHLD
  SIGCONT
  SIGSTOP
  SIGTSTP
  SIGTTIN
  SIGTTOU
  -- The signals described in SUSv2 and POSIX.1-2001.
  SIGBUS
  SIGPOLL
  SIGPROF
  SIGSYS
  SIGTRAP
  SIGURG
  SIGVTALRM
  SIGXCPU
  SIGXFSZ
  -- "Various other signals."
  SIGIOT
  SIGEMT
  SIGSTKFLT
  SIGIO
  SIGCLD
  SIGPWR
  SIGINFO
  SIGLOST
  SIGWINCH
  SIGUNUSED
]]

local map, index = {}, 1
for line in symbols:gmatch '[^\n]+' do
  local name = line:match 'SIG%S+'
  if name then
    map[index] = name
    index = index + 1
  end
end

local signal_table = {}
local signal_handlers = {}
for i, name in ipairs(map) do

  signal_table[i] = string.format([[
# ifdef %s
  { "%s", %s, %s_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif]], name, name, name, name:lower())

  signal_handlers[i] = string.format([[
#ifdef %s
static void %s_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, %i); }
#endif]], name, name:lower(), i - 1)

end

print(string.format([[
/* Generated signal handlers. */

%s

/* Generated signal table. */

static struct {
  const char *name;                     /* Symbolic name of signal (used in Lua)   */
  const int value;                      /* Integer code of signal (used in C)      */
  void (*hook)(lua_State*, lua_Debug*); /* Lua hook to be called by signal handler */
  lua_State *state;                     /* Lua state that installed signal handler */
  lua_Hook oldhook;                     /* Old debug hook and related variables    */
  int oldmask, oldcount;
} known_signals[] = {
%s
};]],
    table.concat(signal_handlers, '\n'),
    table.concat(signal_table, '\n')))
