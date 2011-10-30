--[[

 Detect and install missing system packages required by the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: October 30, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

 LuaRocks doesn't handle installation of system packages and its detection of
 installed system libraries also leaves some to be desired. Because I want to
 make it as easy as possible for people to try out the Lua/APR binding, I wrote
 this Lua script to automatically detect (and optionally install) missing
 system packages on supported platforms. This script is run from the default
 rule in the UNIX makefile.

 Originally this script automatically executed the commands needed to install
 missing system packages when it was executed from the makefile, but the
 LuaRocks maintainers refused to include it and in retrospect it's indeed not
 cool for a makefile to do such things out of the box.
 
 I've now added support for the -n (dry run) argument which is passed from the
 makefile and instructs this script not to install any packages. Instead it
 will just give a friendly suggestion to the user. When the script is executed
 without the -n argument it will go ahead and run the command to install
 missing system packages.

]]

local dry_run = arg[1] == '-n'

-- Miscellaneous functions. {{{1

-- Iterate over whitespace separated words.
local function words(text)
  return text:gmatch '%S+'
end

-- Check whether a given program is executable.
local function executable(program)
  return os.execute('which ' .. program .. ' >/dev/null 2>&1')  == 0
end

-- Get the output of a shell command.
local function readcmd(command)
  local handle = io.popen(command)
  local output = handle:read '*a'
  handle:close()
  return output:match '^%s*(.-)%s*$'
end

-- Print message to standard error.
local function message(text, ...)
  io.stderr:write(text:format(...), '\n')
end

-- Prepend `sudo' to a command only when it's installed and needed.
local function sudo(command, args)
  if executable 'sudo' and readcmd 'id -un' ~= 'root' then
    command = 'sudo ' .. command
  end
  if #args > 0 then
    command = command .. ' ' .. table.concat(args, ' ')
  end
  return command
end

-- Take a shell command that outputs a list of all installed system packages
-- and return a function that executes the command and collects all package
-- names as keys in a table which is then returned.
local function find_packages(command)
  return function()
    local packages = {}
    local handle = io.popen(command)
    for name in handle:lines() do packages[name] = 1 end
    return packages
  end
end

-- Given a list of required system packages, a set of installed packages and a
-- command to install packages, make sure all dependencies are installed.
local function install_packages(required_packages, installed_packages, install_command)
  local missing_packages = {}
  for package_name in words(required_packages) do
    if installed_packages[package_name] then
      message(" - Package `%s' is already installed.", package_name)
    else
      message(" - Missing package `%s'!", package_name)
      table.insert(missing_packages, package_name)
    end
  end
  if #missing_packages > 0 then
    install_command = sudo(install_command, missing_packages)
    if dry_run then
      message([[
The Lua/APR binding has several external dependencies and it looks like some
are missing on your system. You can install them with the following command:

  %s

The build will now continue because it's possible that you don't even want to
build the Lua/APR binding against system packages, however if the build fails
consider executing the command above and then retrying the build.
]], install_command)
    else
      local package_manager = install_command:match '^%S+'
      message("Installing %i missing package(s) using %s ..", #missing_packages, package_manager)
      if sudo(install_command, missing_packages) then
        message("Successfully installed %i missing package(s) using %s.", #missing_packages, package_manager)
      else
        message("Failed to install %i missing package(s) using %s!", #missing_packages, package_manager)
        os.exit(1)
      end
    end
  end
end

-- Debian & Ubuntu. {{{1
-- Tested on Ubuntu Linux 10.04 (Lucid Lynx).

local required_deb_packages = [[
  lua5.1 liblua5.1-0 liblua5.1-0-dev
  libapr1 libapr1-dev
  libaprutil1 libaprutil1-dev libaprutil1-dbd-sqlite3
  libapreq2 libapreq2-dev
]]

local find_deb_packages = find_packages [[
  dpkg --list | awk '/^i/ {print $2}'
]]

-- Red Hat, Fedora Core, Suse Linux & CentOS. {{{1
-- Tested on Fedora Core 15.

local required_rpm_packages = [[
  lua lua-devel
  apr apr-devel
  apr-util apr-util-devel
  libapreq2 libapreq2-devel
]]

local find_rpm_packages = find_packages [[
  rpm -qa --qf '%{NAME}\n'
]]

-- Arch Linux. {{{1
-- Tested on Arch Linux 2011.08.19.

local required_arch_packages = [[
  lua
  apr
  apr-util
  perl-libapreq2
]]

local find_arch_packages = find_packages [[
  bash -c "comm -23 <(pacman -Qeq) <(pacman -Qmq)"
]]

-- }}}1

message "Checking for required system packages .."
if executable 'dpkg' and executable 'apt-get' then
  -- Newer versions of Ubuntu install "recommended packages" by default, but
  -- unfortunately the libapreq2 package recommends libapache2-mod-apreq2 which
  -- pulls in the whole Apache server :-|. I think this is stupid and work
  -- around it by passing the --no-install-recommends command line option
  -- when it seems to be supported (unfortunately "apt-get --help" doesn't list
  -- the supported command line options so we have to grep the binary :-P).
  local install_command = 'apt-get install'
  if os.execute 'strings /usr/bin/apt-get | grep -q install-recommends 1>/dev/null 2>&1' == 0 then
    install_command = install_command .. ' --no-install-recommends'
  end
  install_packages(required_deb_packages, find_deb_packages(), install_command)
elseif executable 'rpm' and executable 'yum' then
  install_packages(required_rpm_packages, find_rpm_packages(), 'yum install')
elseif executable 'pacman' then
  install_packages(required_arch_packages, find_arch_packages(), 'pacman -S')
else
  message "Unknown platform: You'll have to install the required system "
  message "libraries manually (lua, apr, apr-util and libapreq2)."
end
