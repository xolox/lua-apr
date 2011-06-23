/* Signal handling module for the Lua/APR binding.
 *
 * Author: Peter Odding <peter@peterodding.com>
 * Last Change: June 23, 2011
 * Homepage: http://peterodding.com/code/lua/apr/
 * License: MIT
 *
 * [Signals] [signals] provide a limited form of inter-process communication.
 * On UNIX they are for example used to communicate to [daemons] [daemons] that
 * they should reload their configuration or stop gracefully. This module works
 * on Linux and most if not all UNIX systems but it's not very useful on
 * Windows, because [Windows has poor support for signals] [msdn_signals]:
 *
 * > `SIGINT` is not supported for any Win32 application, including Windows
 * > 98/Me and Windows NT/2000/XP. When a CTRL+C interrupt occurs, Win32
 * > operating systems generate a new thread to specifically handle that
 * > interrupt. This can cause a single-thread application such as UNIX,
 * > to become multithreaded, resulting in unexpected behavior.  
 * > ...  
 * > The `SIGILL`, `SIGSEGV`, and `SIGTERM` signals are not generated under
 * > Windows NT. They are included for ANSI compatibility. Thus you can set
 * > signal handlers for these signals with signal(), and you can also
 * > explicitly generate these signals by calling raise().
 *
 * The following signal related functionality is not exposed by the Lua/APR
 * binding because the Apache Portable Runtime doesn't wrap the required
 * functions:
 *
 *  - APR doesn't expose [kill()] [kill] except through `process:kill()` which
 *    only supports two signals (`SIGTERM` and `SIGKILL`) which means users of
 *    the Lua/APR binding cannot send arbitrary signals to processes (use
 *    [luaposix] [lposix] instead)
 *
 *  - APR doesn't expose [alarm()][alarm] and Windows doesn't support it which
 *    means the `SIGALRM` signal is useless (use [lalarm] [lalarm] instead)
 *
 * [signals]: http://en.wikipedia.org/wiki/Signal_(computing)
 * [daemons]: http://en.wikipedia.org/wiki/Daemon_(computing)
 * [msdn_signals]: http://msdn.microsoft.com/en-us/library/xdkz3x12(v=vs.71).aspx
 * [kill]: http://linux.die.net/man/3/kill
 * [lposix]: http://luaforge.net/projects/luaposix/
 * [alarm]: http://linux.die.net/man/3/alarm
 * [lalarm]: http://www.tecgraf.puc-rio.br/~lhf/ftp/lua/#lalarm
 */

#include "lua_apr.h"
#include <apr_signal.h>

/* Internal functions. {{{1 */

#define REGISTRYKEY "Lua/APR signal handlers table"
static void generic_hook(lua_State*, lua_Debug*, int);

/* Generated Lua hook handlers. {{{2 */

#ifdef SIGHUP
static void sighup_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 0); }
#endif
#ifdef SIGINT
static void sigint_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 1); }
#endif
#ifdef SIGQUIT
static void sigquit_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 2); }
#endif
#ifdef SIGILL
static void sigill_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 3); }
#endif
#ifdef SIGABRT
static void sigabrt_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 4); }
#endif
#ifdef SIGFPE
static void sigfpe_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 5); }
#endif
#ifdef SIGKILL
static void sigkill_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 6); }
#endif
#ifdef SIGSEGV
static void sigsegv_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 7); }
#endif
#ifdef SIGPIPE
static void sigpipe_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 8); }
#endif
#ifdef SIGALRM
static void sigalrm_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 9); }
#endif
#ifdef SIGTERM
static void sigterm_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 10); }
#endif
#ifdef SIGUSR1
static void sigusr1_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 11); }
#endif
#ifdef SIGUSR2
static void sigusr2_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 12); }
#endif
#ifdef SIGCHLD
static void sigchld_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 13); }
#endif
#ifdef SIGCONT
static void sigcont_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 14); }
#endif
#ifdef SIGSTOP
static void sigstop_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 15); }
#endif
#ifdef SIGTSTP
static void sigtstp_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 16); }
#endif
#ifdef SIGTTIN
static void sigttin_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 17); }
#endif
#ifdef SIGTTOU
static void sigttou_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 18); }
#endif
#ifdef SIGBUS
static void sigbus_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 19); }
#endif
#ifdef SIGPOLL
static void sigpoll_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 20); }
#endif
#ifdef SIGPROF
static void sigprof_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 21); }
#endif
#ifdef SIGSYS
static void sigsys_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 22); }
#endif
#ifdef SIGTRAP
static void sigtrap_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 23); }
#endif
#ifdef SIGURG
static void sigurg_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 24); }
#endif
#ifdef SIGVTALRM
static void sigvtalrm_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 25); }
#endif
#ifdef SIGXCPU
static void sigxcpu_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 26); }
#endif
#ifdef SIGXFSZ
static void sigxfsz_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 27); }
#endif
#ifdef SIGIOT
static void sigiot_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 28); }
#endif
#ifdef SIGEMT
static void sigemt_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 29); }
#endif
#ifdef SIGSTKFLT
static void sigstkflt_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 30); }
#endif
#ifdef SIGIO
static void sigio_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 31); }
#endif
#ifdef SIGCLD
static void sigcld_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 32); }
#endif
#ifdef SIGPWR
static void sigpwr_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 33); }
#endif
#ifdef SIGINFO
static void siginfo_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 34); }
#endif
#ifdef SIGLOST
static void siglost_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 35); }
#endif
#ifdef SIGWINCH
static void sigwinch_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 36); }
#endif
#ifdef SIGUNUSED
static void sigunused_hook(lua_State *L, lua_Debug *ar) { generic_hook(L, ar, 37); }
#endif

/* Generated signal table. {{{2 */

static struct {
  const char *name;                     /* Symbolic name of signal (used in Lua)   */
  const int value;                      /* Integer code of signal (used in C)      */
  void (*hook)(lua_State*, lua_Debug*); /* Lua hook to be called by signal handler */
  lua_State *state;                     /* Lua state that installed signal handler */
  lua_Hook oldhook;                     /* Old debug hook and related variables    */
  int oldmask, oldcount;
} signaltable[] = {
# ifdef SIGHUP
  { "SIGHUP", SIGHUP, sighup_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGINT
  { "SIGINT", SIGINT, sigint_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGQUIT
  { "SIGQUIT", SIGQUIT, sigquit_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGILL
  { "SIGILL", SIGILL, sigill_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGABRT
  { "SIGABRT", SIGABRT, sigabrt_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGFPE
  { "SIGFPE", SIGFPE, sigfpe_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGKILL
  { "SIGKILL", SIGKILL, sigkill_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGSEGV
  { "SIGSEGV", SIGSEGV, sigsegv_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGPIPE
  { "SIGPIPE", SIGPIPE, sigpipe_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGALRM
  { "SIGALRM", SIGALRM, sigalrm_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGTERM
  { "SIGTERM", SIGTERM, sigterm_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGUSR1
  { "SIGUSR1", SIGUSR1, sigusr1_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGUSR2
  { "SIGUSR2", SIGUSR2, sigusr2_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGCHLD
  { "SIGCHLD", SIGCHLD, sigchld_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGCONT
  { "SIGCONT", SIGCONT, sigcont_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGSTOP
  { "SIGSTOP", SIGSTOP, sigstop_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGTSTP
  { "SIGTSTP", SIGTSTP, sigtstp_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGTTIN
  { "SIGTTIN", SIGTTIN, sigttin_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGTTOU
  { "SIGTTOU", SIGTTOU, sigttou_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGBUS
  { "SIGBUS", SIGBUS, sigbus_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGPOLL
  { "SIGPOLL", SIGPOLL, sigpoll_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGPROF
  { "SIGPROF", SIGPROF, sigprof_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGSYS
  { "SIGSYS", SIGSYS, sigsys_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGTRAP
  { "SIGTRAP", SIGTRAP, sigtrap_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGURG
  { "SIGURG", SIGURG, sigurg_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGVTALRM
  { "SIGVTALRM", SIGVTALRM, sigvtalrm_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGXCPU
  { "SIGXCPU", SIGXCPU, sigxcpu_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGXFSZ
  { "SIGXFSZ", SIGXFSZ, sigxfsz_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGIOT
  { "SIGIOT", SIGIOT, sigiot_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGEMT
  { "SIGEMT", SIGEMT, sigemt_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGSTKFLT
  { "SIGSTKFLT", SIGSTKFLT, sigstkflt_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGIO
  { "SIGIO", SIGIO, sigio_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGCLD
  { "SIGCLD", SIGCLD, sigcld_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGPWR
  { "SIGPWR", SIGPWR, sigpwr_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGINFO
  { "SIGINFO", SIGINFO, siginfo_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGLOST
  { "SIGLOST", SIGLOST, siglost_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGWINCH
  { "SIGWINCH", SIGWINCH, sigwinch_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
# ifdef SIGUNUSED
  { "SIGUNUSED", SIGUNUSED, sigunused_hook, NULL, NULL, 0, 0 },
# else
  { NULL, 0, NULL, NULL, NULL, 0, 0 },
# endif
};

/* Name to table index mapping. {{{2 */

static int check_signal_name(lua_State *L, int idx)
{
  int sigidx;
  const char *name;

  name = luaL_checkstring(L, idx);
  for (sigidx = 0; sigidx < count(signaltable) - 1; sigidx++)
    if (signaltable[sigidx].name != NULL && strcmp(name, signaltable[sigidx].name) == 0)
      return sigidx;

  return luaL_argerror(L, 1,
      lua_pushfstring(L, "signal %s not supported", name));
}

/* Generic Lua hook. {{{2 */

static void generic_hook(lua_State *L, lua_Debug *ar, int sigidx)
{
  int top = lua_gettop(L);
  lua_sethook(L,
      signaltable[sigidx].oldhook,
      signaltable[sigidx].oldmask,
      signaltable[sigidx].oldcount);
  lua_pushliteral(L, REGISTRYKEY);
  lua_gettable(L, LUA_REGISTRYINDEX);
  if (lua_istable(L, -1)) {
    lua_rawgeti(L, -1, sigidx);
    if (lua_type(L, -1) == LUA_TFUNCTION)
      lua_call(L, 0, 0);
  }
  lua_settop(L, top);
}

/* Signal handler function. {{{2 */

static void signal_function(int signo)
{
  int sigidx;

  /* I've tried speeding this up by generating a switch statement but got
   * duplicate value errors and decided that it wasn't worth the trouble. */

  for (sigidx = 0; sigidx < count(signaltable) - 1; sigidx++) {
    if (signo == signaltable[sigidx].value) {
      signaltable[sigidx].oldhook = lua_gethook(signaltable[sigidx].state);
      signaltable[sigidx].oldmask = lua_gethookmask(signaltable[sigidx].state);
      signaltable[sigidx].oldcount = lua_gethookcount(signaltable[sigidx].state);
      lua_sethook(signaltable[sigidx].state, signaltable[sigidx].hook, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
      break;
    }
  }
}

/* apr.signal(name [, handler]) -> status {{{1
 *
 * Set the signal handler function for a given signal. The argument @name must
 * be a string with the name of the signal to handle (`apr.signal_names()`
 * returns a table of available names on your platform). The argument @handler
 * must be of type function, unless it is nil in which case default handling
 * is restored for the given signal. On success true is returned, otherwise a
 * nil followed by an error message is returned.
 */

int lua_apr_signal(lua_State *L)
{
  int sigidx;

  /* Check arguments. */
  sigidx = check_signal_name(L, 1);
  if (!lua_isnil(L, 2))
    luaL_checktype(L, 2, LUA_TFUNCTION);

  /* Get the Lua table with signal handlers. */
  lua_pushliteral(L, REGISTRYKEY);
  lua_gettable(L, LUA_REGISTRYINDEX);
  if (lua_isnil(L, -1)) {
    /* Table didn't exist yet, create it. */
    lua_pop(L, 1);
    lua_newtable(L);
    /* Add the table to the registry. */
    lua_pushliteral(L, REGISTRYKEY);
    lua_pushvalue(L, -2);
    lua_rawset(L, LUA_REGISTRYINDEX);
  }

  /* Save or clear the Lua side signal handler function in the table. */
  lua_pushvalue(L, 2);
  lua_rawseti(L, -2, sigidx);

  /* Remember the Lua state that installed the signal handler. */
  signaltable[sigidx].state = L;

  /* Install or clear the C side signal handler function. */
  lua_pushboolean(L, apr_signal(signaltable[sigidx].value,
        lua_isnil(L, 2) ? SIG_DFL : signal_function) != SIG_ERR);

  return 1;
}

/* apr.signal_raise(name) -> status {{{1
 *
 * Send a signal to *the current process*. The result is true when the call
 * succeeded, false otherwise. This function is useful to test your own signal
 * handlers:
 *
 *     > = apr.signal('SIGSEGV', function() print 'almost crashed :-)' end)
 *     true
 *     > = apr.signal_raise('SIGSEGV')
 *     almost crashed :-)
 *     true
 *     > = apr.signal('SIGSEGV', nil)
 *     true
 *     > = apr.signal_raise('SIGSEGV')
 *     zsh: segmentation fault  lua
 */

int lua_apr_signal_raise(lua_State *L)
{
  int sigidx, status;

  sigidx = check_signal_name(L, 1);
  status = raise(signaltable[sigidx].value);
  lua_pushboolean(L, status == 0);

  return 1;
}

/* apr.signal_block(name) -> status {{{1
 *
 * Block the delivery of a particular signal. On success true is returned,
 * otherwise a nil followed by an error message is returned.
 */

int lua_apr_signal_block(lua_State *L)
{
  apr_status_t status;
  int sigidx;

  sigidx = check_signal_name(L, 1);
  status = apr_signal_block(signaltable[sigidx].value);

  return push_status(L, status);
}

/* apr.signal_unblock(name) -> status {{{1
 *
 * Enable the delivery of a particular signal. On success true is returned,
 * otherwise a nil followed by an error message is returned.
 */

int lua_apr_signal_unblock(lua_State *L)
{
  apr_status_t status;
  int sigidx;

  sigidx = check_signal_name(L, 1);
  status = apr_signal_unblock(signaltable[sigidx].value);

  return push_status(L, status);
}

/* apr.signal_names() -> names {{{1
 *
 * Return a table with available signal names on your platform. The keys of the
 * table are the names of the signals and the values are the integer codes
 * associated with the signals.
 *
 * As the previous paragraph implies the result of this function can differ
 * depending on your operating system and processor architecture. For example
 * on my Ubuntu Linux 10.04 installation I get these results:
 *
 *     > signals = {}
 *     > for k, v in pairs(apr.signal_names()) do
 *     >>  table.insert(signals, { name = k, value = v })
 *     >> end
 *     > table.sort(signals, function(a, b)
 *     >>  return a.value < b.value
 *     >> end)
 *     > for _, s in ipairs(signals) do
 *     >>  print(string.format('% 2i: %s', s.value, s.name))
 *     >> end
 *      1: SIGHUP
 *      2: SIGINT
 *      3: SIGQUIT
 *      4: SIGILL
 *      5: SIGTRAP
 *      6: SIGIOT
 *      6: SIGABRT
 *      7: SIGBUS
 *      8: SIGFPE
 *      9: SIGKILL
 *     10: SIGUSR1
 *     11: SIGSEGV
 *     12: SIGUSR2
 *     13: SIGPIPE
 *     14: SIGALRM
 *     15: SIGTERM
 *     16: SIGSTKFLT
 *     17: SIGCHLD
 *     17: SIGCLD
 *     18: SIGCONT
 *     19: SIGSTOP
 *     20: SIGTSTP
 *     21: SIGTTIN
 *     22: SIGTTOU
 *     23: SIGURG
 *     24: SIGXCPU
 *     25: SIGXFSZ
 *     26: SIGVTALRM
 *     27: SIGPROF
 *     28: SIGWINCH
 *     29: SIGPOLL
 *     29: SIGIO
 *     30: SIGPWR
 *     31: SIGSYS
 *
 * After creating the above table I was surprised to see several numbers which
 * have two names in the above output, but after looking up the names it turns
 * out that these are just synonyms.
 *
 * Note that just because a signal is included in this table doesn't
 * necessarily mean the signal is usable from Lua! For example [SIGALRM]
 * [sigalrm] is only useful when you can call the alarm() function defined by
 * [POSIX][posix] but that function isn't exposed by the Apache Portable
 * Runtime (you can use [lalarm] [lalarm] instead in this case).
 *
 * [sigalrm]: http://en.wikipedia.org/wiki/SIGALRM
 * [posix]: http://en.wikipedia.org/wiki/POSIX
 */

int lua_apr_signal_names(lua_State *L)
{
  int sigidx;

  lua_newtable(L);
  for (sigidx = 0; sigidx < count(signaltable) - 1; sigidx++) {
    if (signaltable[sigidx].name != NULL) {
      lua_pushstring(L, signaltable[sigidx].name);
      lua_pushinteger(L, signaltable[sigidx].value);
      lua_rawset(L, -3);
    }
  }

  return 1;
}
