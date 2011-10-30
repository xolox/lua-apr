--[[

 Lua source code for the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: October 30, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

 This Lua script is executed on require("apr"), loads the binary module using
 require("apr.core"), defines several library functions implemented on top of
 the binary module and returns the module table as the result of require().

--]]

local apr = require 'apr.core'
apr._VERSION = '0.20.6'

-- apr.md5(input [, binary]) -> digest {{{1
--
-- Calculate the [MD5] [md5] message digest of the string @input. On success
-- the digest is returned as a string of 32 hexadecimal characters, or a string
-- of 16 bytes if @binary evaluates to true. Otherwise a nil followed by an
-- error message is returned.
--
-- *This function is binary safe.*
--
-- Part of the "Cryptography routines" module.

function apr.md5(input, binary)
  assert(type(input) == 'string', "bad argument #1 to apr.md5() (string expected)")
  local context, digest, status, errmsg, errcode
  context, errmsg, errcode = apr.md5_init()
  if context then
    status, errmsg, errcode = context:update(input)
    if status then
      digest, errmsg, errcode = context:digest(binary)
      if digest then return digest end
    end
  end
  return nil, errmsg, errcode
end

-- apr.sha1(input [, binary]) -> digest {{{1
--
-- Calculate the [SHA1] [sha1] message digest of the string @input. On success
-- the digest is returned as a string of 40 hexadecimal characters, or a string
-- of 20 bytes if @binary evaluates to true. Otherwise a nil followed by an
-- error message is returned.
--
-- *This function is binary safe.*
--
-- Part of the "Cryptography routines" module.

function apr.sha1(input, binary)
  assert(type(input) == 'string', "bad argument #1 to apr.sha1() (string expected)")
  local context, digest, status, errmsg, errcode
  context, errmsg, errcode = apr.sha1_init()
  if context then
    status, errmsg, errcode = context:update(input)
    if status then
      digest, errmsg, errcode = context:digest(binary)
      if digest then return digest end
    end
  end
  return nil, errmsg, errcode
end

-- apr.filepath_which(program [, find_all]) -> pathname {{{1
--
-- Find the full pathname of @program by searching the directories in the
-- [$PATH] [path_var] environment variable and return the pathname of the
-- first program that's found. If @find_all is true then a list with the
-- pathnames of all matching programs is returned instead.
--
-- [path_var]: http://en.wikipedia.org/wiki/PATH_%28variable%29
--
-- Part of the "File path manipulation" module.

function apr.filepath_which(program, find_all)
  local split = apr.filepath_list_split
  local is_windows = apr.platform_get() == 'WIN32'
  local extensions = is_windows and split(apr.env_get 'PATHEXT')
  local results = find_all and {}
  for _, directory in ipairs(split(apr.env_get 'PATH')) do
    local candidate = apr.filepath_merge(directory, program)
    if apr.stat(candidate, 'type') == 'file' then
      -- TODO if not is_windows check executable bits
      if not find_all then return candidate end
      results[#results + 1] = candidate
    end
    if is_windows and #extensions >= 1 then
      for _, extension in ipairs(extensions) do
        candidate = apr.filepath_merge(directory, program .. '.' .. extension)
        if apr.stat(candidate, 'type') == 'file' then
          if not find_all then return candidate end
          results[#results + 1] = candidate
        end
      end
    end
  end
  return results
end

-- apr.glob(pattern [, ignorecase]) -> iterator {{{1
--
-- Split @pattern into a directory path and a filename pattern and return an
-- iterator which returns all filenames in the directory that match the
-- extracted filename pattern. The `apr.fnmatch()` function is used for
-- filename matching so the documentation there applies.
--
-- *This function is not binary safe.*
--
-- Part of the "Filename matching" module.

function apr.glob(pattern, ignorecase)
  local fnmatch = apr.fnmatch
  local yield = coroutine.yield
  local directory, pattern = apr.filepath_parent(pattern)
  local handle = assert(apr.dir_open(directory))
  return coroutine.wrap(function()
    for path, name in handle:entries('path', 'name') do
      if fnmatch(pattern, name, ignorecase) then
        yield(path)
      end
    end
    handle:close()
  end)
end

-- apr.getopt(usage [, config ]) -> options, arguments {{{1
--
-- Parse the [command line arguments] [cmdargs] according to the option letters
-- and/or long options defined in the string @usage (see the example below) and
-- return a table with the matched options and a table with any remaining
-- positional arguments. When an option is matched multiple times, the
-- resulting value in @options depends on the following context:
--
--  * If the option doesn't take an argument, the value will be a number
--    indicating the number of times that the option was matched
--
--  * If the option takes an argument and only one option/argument pair is
--    matched, the value will be the argument (a string). When more than one
--    pair is matched for the same option letter/name, the values will be
--    collected in a table
--
-- The optional @config table can be used to change the following defaults:
--
--  * When @usage mentions `-h` or `--help` and either of these options is
--    matched in the arguments, `apr.getopt()` will print the usage message
--    and call `os.exit()`. To avoid this set `config.show_usage` to false
--    (not nil!)
--
--  * When an error is encountered during argument parsing, `apr.getopt()` will
--    print a warning about the invalid argument and call `os.exit()`. To avoid
--    this set `config.handle_errors` to false (not nil!)
--
--  * By default the arguments in the global variable [arg] [arg-global] will
--    be used, but you can set `config.args` to a table of arguments to be
--    used instead
--
-- Here is a short example of a valid Lua script that doesn't really do
-- anything useful but demonstrates the use of `apr.getopt()`:
--
--     apr = require 'apr'
--     opts, args = apr.getopt [[
--     Usage: echo.lua [OPTIONS] ARG...
--       -h, --help     show this message and exit
--       -v, --verbose  make more noise
--           --version  print version and exit
--     ]]
--     if opts.version then
--       print "This is version 0.1"
--     else
--       if opts.verbose then
--         print("Got", #args, "arguments")
--       end
--       if opts.verbose >= 2 then
--         print "Here they are:"
--       end
--       for i = 1, #args do print(args[i]) end
--     end
--
-- The `apr.getopt()` function is very similar to [Lapp] [lapp] by Steve
-- Donovan although Lapp is more full featured, for example it validates and
-- converts argument types.
--
-- [cmdargs]: http://en.wikipedia.org/wiki/Command-line_argument
-- [arg-global]: http://www.lua.org/manual/5.1/manual.html#6
-- [lapp]: http://lua-users.org/wiki/LappFramework
--
-- Part of the "Command argument parsing" module.

-- Match an option letter or a long option at the start of the line.
local function match_arg(table, key, line, pattern)
  local opt, remainder = line:match(pattern)
  if opt and remainder then
    local name, args = opt:match '^(.-)=(.-)$'
    if name and args then
      table[key] = name
      table.has_arg = true
    else
      if key == 'optch' then
        opt = opt:gsub(',$', '')
      end
      table[key] = opt
    end
    return remainder
  end
  return line
end

local function parse_usage(usage)
  local optdefs = {}
  -- Parse the usage message into a nested table structure.
  for line in usage:gmatch '[^\n]+' do
    local t = {}
    line = match_arg(t, 'optch', line, '^%s*%-([^-]%S*)(.-)$')
    line = match_arg(t, 'name', line, '^%s*%-%-(%S+)(.-)$')
    t.description = line:match '^%s*(.-)%s*$'
    optdefs[#optdefs + 1] = t
    if t.optch then optdefs[t.optch] = t end
    if t.name then optdefs[t.name] = t end
  end
  -- Generate any missing "optch" values.
  for i, t in ipairs(optdefs) do
    if t.name and not t.optch then
      for i = 1, 255 do
        local c = string.char(i)
        if not optdefs[c] then
          t.optch = c
          t.fake_optch = true
          optdefs[c] = t
          break
        end
      end
    end
  end
  return optdefs
end

local real_getopt = apr.getopt

function apr.getopt(usage, config)
  -- Validate arguments.
  assert(type(usage) == 'string')
  if config then assert(type(config) == 'table') else config = {} end
  local arguments = config.args or _G.arg
  assert(type(arguments) == 'table', "No arguments to parse!")
  assert(arguments[0], "Program name missing from arguments!")
  -- Get the option definitions from the usage message.
  local optdefs = parse_usage(usage)
  -- Parse the Lua script's arguments using the definitions.
  local opts, args, code = real_getopt(optdefs, arguments, config.handle_errors == false)
  -- Handle errors during command argument parsing.
  if not (opts and args) then
    if config.handle_errors ~= false then
      os.exit(1)
    else
      local msg = args
      return nil, msg, code
    end
  end
  -- Copy option letter values to long option aliases.
  for i, t in ipairs(optdefs) do
    if t.optch and t.name then
      opts[t.name] = opts[t.optch]
    end
    if t.fake_optch then
      opts[t.optch] = nil
    end
  end
  -- Print usage message or return results.
  if config.show_usage ~= false and (opts.h or opts.help) then
    io.write(usage)
    os.exit(0)
  else
    return opts, args
  end
end

if apr.thread_create then

-- apr.thread(body [, arg, ...]) -> thread {{{1
--
-- Shortcut for `apr.thread_create()` that supports actual functions as @body
-- by converting them to byte code using `string.dump()`. This means you don't
-- lose syntax highlighting and debug information. This also calls `assert()`
-- on the results of `apr.thread_create()`. This function won't work in Lua
-- implementations like LuaJIT 2 where `string.dump()` is not available.
--
-- Part of the "Multi threading" module.

function apr.thread(body, ...)
  if type(body) == 'function' then
    body = string.dump(body)
  end
  return assert(apr.thread_create(body, ...))
end

end

-- }}}1

return apr

-- vim: ts=2 sw=2 et tw=79 fen fdm=marker
