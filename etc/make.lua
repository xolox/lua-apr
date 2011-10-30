--[[

 Supporting code for the UNIX makefile of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: October 30, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

 This Lua script enhances the UNIX makefile of the Lua/APR binding in two ways,
 both aiming to make the build process more portable:

  - The external programs pkg-config, apr-1-config, apu-1-config and
    apreq2-config are used to find the compiler and linker flags
    for the Lua 5.1, APR 1, APR-util 1 and libapreq2 libraries.

  - It knows about needed system packages on Debian, Ubuntu,
    Arch Linux, Red Hat, Fedora Core, Suse Linux and CentOS.

 The makefile always invokes this script to find compiler and linker flags, but
 only when the build fails will the script be called to perform a "sanity
 check" of the external dependencies / system packages.

]]

-- Miscellaneous functions. {{{1

-- trim() -- Trim leading/trailing whitespace from string. {{{2

local function trim(s)
  return s:match '^%s*(.-)%s*$'
end

-- dedent() -- remove common leading whitespace from string. {{{2

local function dedent(text)
  local pattern = '\n' .. text:match '^[ \t]*'
  return trim(('\n' .. text):gsub(pattern, '\n'))
end

-- message() -- Print formatted message to standard error. {{{2

local function message(text, ...)
  io.stderr:write(dedent(text:format(...)), '\n')
  io.stderr:flush()
end

-- words() -- Iterate over whitespace separated words. {{{2

local function words(text)
  return text:gmatch '%S+'
end

-- readfile() -- Read the contents of a file into a string. {{{2

local function readfile(path)
  local handle = io.open(path)
  local content = handle and handle:read '*a' or ''
  if handle then handle:close() end
  return content
end

-- executable() -- Check whether a given program is executable. {{{2

local function executable(program)
  return os.execute('which ' .. program .. ' >/dev/null 2>&1')  == 0
end

-- readcmd() -- Get the results of a shell command (status, stdout, stderr). {{{2

local function readcmd(command)
  local stdout = os.tmpname()
  local stderr = os.tmpname()
  local status = os.execute(command
                  .. ' 1>"' .. stdout .. '"'
                  .. ' 2>"' .. stderr .. '"')
  return status, readfile(stdout), readfile(stderr)
end

-- readstdout() -- Get the standard output of a shell command. {{{2

local function readstdout(command)
  local status, stdout, stderr = readcmd(command)
  return status == 0 and stdout or ''
end

-- sudo() -- Prepend `sudo' to a command when `sudo' is installed. {{{2

local function sudo(command, args)
  if executable 'sudo' and (os.getenv 'SUDO_USER'
      or readstdout 'id -un' ~= 'root') then
    command = 'sudo ' .. command
  end
  if #args > 0 then
    command = command .. ' ' .. table.concat(args, ' ')
  end
  return command
end

-- mergeflags() -- Merge command line flags reported by pkg-config. {{{2

local function mergeflags(flags, command)
  local status, stdout, stderr = readcmd(command)
  if status == 0 then
    for flag in words(stdout) do
      if not flags[flag] then
        local position = #flags + 1
        flags[flag] = position
        flags[position] = flag
      end
    end
  end
end

-- getcflags() -- Get the compiler flags needed to build the Lua/APR binding. {{{1

local function getcflags()
  local flags, count = {}, 0
  -- Compiler flags for Lua 5.1.
  mergeflags(flags, 'pkg-config --cflags lua5.1') -- Debian/Ubuntu
  mergeflags(flags, 'pkg-config --cflags lua-5.1') -- FreeBSD
  mergeflags(flags, 'pkg-config --cflags lua') -- Arch Linux
  if #flags == 0 then
    message "Warning: Failed to determine Lua 5.1 compiler flags."
  end
  count = #flags
  -- Compiler flags for APR 1.
  mergeflags(flags, 'apr-1-config --cflags --cppflags --includes') -- sometimes this is available
  mergeflags(flags, 'pkg-config --cflags apr-1') -- otherwise we fall back to pkg-config
  if #flags == count then
    message "Warning: Failed to determine APR 1 compiler flags."
  end
  count = #flags
  -- Compiler flags for APR-util 1.
  mergeflags(flags, 'apu-1-config --includes') -- sometimes this is available
  mergeflags(flags, 'pkg-config --cflags apr-util-1') -- otherwise we fall back to pkg-config
  if #flags == count then
    message "Warning: Failed to determine APR-util 1 compiler flags."
  end
  count = #flags
  -- Compiler flags for libapreq2.
  mergeflags(flags, 'apreq2-config --includes')
  local have_apreq = (#flags > count)
  if not have_apreq then
    message "Warning: Failed to determine libapreq2 compiler flags."
  end
  -- Let the C source code know whether libapreq2 is available.
  flags[#flags + 1] = '-DLUA_APR_HAVE_APREQ=' .. (have_apreq and 1 or 0)
  return table.concat(flags, ' ')
end

-- getlflags() -- Get the linker flags needed to build the Lua/APR binding. {{{1

local function getlflags()
  local flags = {}
  -- Linker flags for APR 1.
  mergeflags(flags, 'apr-1-config --link-ld --libs')
  mergeflags(flags, 'pkg-config --libs apr-1')
  if #flags == 0 then
    message "Warning: Failed to determine APR 1 linker flags."
  end
  count = #flags
  -- Linker flags for APR-util 1.
  mergeflags(flags, 'apu-1-config --link-ld --libs --ldap-libs')
  mergeflags(flags, 'pkg-config --libs apr-util-1')
  if #flags == count then
    message "Warning: Failed to determine APR-util 1 linker flags."
  end
  count = #flags
  -- Linker flags for libapreq2.
  mergeflags(flags, 'apreq2-config --link-ld')
  if #flags == 0 then
    message "Warning: Failed to determine apreq2 linker flags."
  end
  return table.concat(flags, ' ')
end

-- checkpackages() -- Check if required system packages are installed. {{{1

local function delimiter()
  message(string.rep('-', 79))
end

local function checkpackages()

  -- Let the user know what's going on.
  delimiter()
  message [[
    It looks like your Lua/APR build failed. This could be because you're
    missing some external dependencies. I will now check if you have the
    required system packages installed (this should work on Debian, Ubuntu,
    Arch Linux, Red Hat, Fedora Core, Suse Linux and CentOS).
  ]]
  delimiter()

  -- Perform platform detection.
  local command, requiredpackages, installedpackagescmd
  if executable 'dpkg' and executable 'apt-get' then
    -- Debian & Ubuntu.
    -- Tested on Ubuntu Linux 10.04 (Lucid Lynx).
    command = 'apt-get install'
    installedpackagescmd = [[ dpkg --list | awk '/^i/ {print $2}' ]]
    requiredpackages = [[
      lua5.1 liblua5.1-0 liblua5.1-0-dev
      libapr1 libapr1-dev
      libaprutil1 libaprutil1-dev libaprutil1-dbd-sqlite3
      libapreq2 libapreq2-dev
    ]]
    -- Newer versions of Ubuntu install "recommended packages" by default, but
    -- unfortunately the libapreq2 package recommends libapache2-mod-apreq2 which
    -- pulls in the whole Apache server :-|. I think this is stupid and work
    -- around it by passing the --no-install-recommends command line option
    -- when it seems to be supported (unfortunately "apt-get --help" doesn't list
    -- the supported command line options so we have to grep the binary :-P).
    if os.execute 'strings /usr/bin/apt-get | grep -q install-recommends 1>/dev/null 2>&1' == 0 then
      command = command .. ' --no-install-recommends'
    end
  elseif executable 'rpm' and executable 'yum' then
    -- Red Hat, Fedora Core, Suse Linux & CentOS.
    -- Tested on Fedora Core 15.
    command = 'yum install'
    installedpackagescmd = [[ rpm -qa --qf '%{NAME}\n' ]]
    requiredpackages = [[
      lua lua-devel
      apr apr-devel
      apr-util apr-util-devel
      libapreq2 libapreq2-devel
    ]]
  elseif executable 'pacman' then
    -- Arch Linux.
    -- Tested on Arch Linux 2011.08.19.
    command = 'pacman -S'
    installedpackagescmd = [[ bash -c "comm -23 <(pacman -Qeq) <(pacman -Qmq)" ]]
    requiredpackages = [[ lua apr apr-util perl-libapreq2 ]]
  else
    message [[
      Unknown platform: You'll have to install the required system
      packages manually (lua, apr, apr-util and libapreq2).
    ]]
    delimiter()
    os.exit(1)
  end

  -- Find the installed packages.
  local installedpackages = {}
  local handle = io.popen(installedpackagescmd)
  for name in handle:lines() do
    installedpackages[name] = true
  end

  -- Determine missing packages.
  local missingpackages = {}
  for name in words(requiredpackages) do
    if installedpackages[name] then
      message(" - Package `%s' is already installed.", name)
    else
      message(" - Missing package `%s'!", name)
      table.insert(missingpackages, name)
    end
  end

  -- If we found any missing system packages, suggest to the user how the
  -- missing packages can be installed.
  delimiter()
  if #missingpackages == 0 then
    message [[
      It looks like you have all required external dependencies installed,
      something else must have gone wrong...
    ]]
  else
    message([[
      The Lua/APR binding has several external dependencies and it looks like
      some are missing on your system. You should be able to install the
      dependencies with the following command:

        %s

      ]], sudo(command, missingpackages))
  end
  delimiter()

  -- Because this script is executed from the makefile in || expressions we
  -- always exit with a non-zero exit code, so that the build still fails.
  os.exit(1)

end

-- }}}1

if arg[1] == '--cflags' then
  print(getcflags())
elseif arg[1] == '--lflags' then
  print(getlflags())
elseif arg[1] == '--check' then
  checkpackages()
else
  message("Invalid option!")
  os.exit(1)
end
