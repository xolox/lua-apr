--[[

 Unit tests for command argument parsing module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: March 27, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

local status, apr = pcall(require, 'apr')
if not status then
  pcall(require, 'luarocks.require')
  apr = require 'apr'
end
local helpers = require 'apr.test.helpers' 

local function testopts(context)
  context.arguments[0] = 'getopt'
  local config = { silent = false, args = context.arguments }
  local opts, args = assert(apr.getopt(context.usage, config))
  assert(helpers.deepequal(opts, context.expected_opts))
  assert(helpers.deepequal(args, context.expected_args))
end

-- Short options (option letters) only.
testopts {
  usage = [[
    -s    only an option letter
    -a=?  short option with argument
  ]],
  arguments = { 'arg1', '-s', 'arg2', '-a5' },
  expected_opts = { s = 1, a = '5' },
  expected_args = { 'arg1', 'arg2' }
}

-- Long option names only.
testopts {
  usage = [[
    --long   long option name
    --arg=?  long option with argument
  ]],
  arguments = { '--long', 'arg1', '--arg=yes', 'arg2' },
  expected_opts = { long = 1, arg = 'yes' },
  expected_args = { 'arg1', 'arg2' }
}

-- Options with aliases.
testopts {
  usage = [[
    -b, --both   option letter and name
    -a, --arg=?  aliased + argument
  ]],
  arguments = { '-b', 'arg1', '--arg=yes', 'arg2' },
  expected_opts = { b = 1, both = 1, a = 'yes', arg = 'yes' },
  expected_args = { 'arg1', 'arg2' }
}

-- Repeating options.
testopts {
  usage = [[
    -v, --verbose  increase verbosity
    -a, --arg=?    add value to list
  ]],
  arguments = { '-vv', 'a1', '--verbose', 'a2', '-av1', '-a', 'v2', '--arg', 'v3', '--arg=v4', 'a3' },
  expected_opts = { v = 3, verbose = 3, a = { 'v1', 'v2', 'v3', 'v4' }, arg = { 'v1', 'v2', 'v3', 'v4' } },
  expected_args = { 'a1', 'a2', 'a3' }
}
