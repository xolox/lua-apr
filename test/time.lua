--[[

 Unit tests for the time routines module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: June 16, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

--]]

local status, apr = pcall(require, 'apr')
if not status then
  pcall(require, 'luarocks.require')
  apr = require 'apr'
end

-- Based on http://svn.apache.org/viewvc/apr/apr/trunk/test/testtime.c?view=markup.

local now = 1032030336186711 / 1000000

-- Check that apr.time_now() more or less matches os.time()
assert(math.abs(os.time() - apr.time_now()) <= 2)

-- apr.time_explode() using apr_time_exp_lt()
local posix_exp = os.date('*t', now)
local xt = assert(apr.time_explode(now)) -- apr_time_exp_lt()
for k, v in pairs(posix_exp) do assert(v == xt[k]) end

-- apr.time_implode() on a local time table (ignores floating point precision
-- because on my laptop "now" equals 1032030336.186711 while "imp" equals
-- 1032037536.186710)
local imp = assert(apr.time_implode(xt)) -- apr_time_exp_gmt_get()
assert(math.floor(now) == math.floor(imp))

-- apr.time_implode() on a GMT time table
local xt = assert(apr.time_explode(now, true)) -- apr_time_exp_gmt()
local imp = assert(apr.time_implode(xt)) -- apr_time_exp_gmt_get()
assert(math.floor(now) == math.floor(imp))

-- Test apr.time_format() (example from http://en.wikipedia.org/wiki/Unix_time)
assert(apr.time_format('rfc822', 1000000000) == 'Sun, 09 Sep 2001 01:46:40 GMT')
assert(apr.time_format('%Y-%m', 1000000000) == '2001-09')

-- Test that apr.sleep() supports sub second resolution.
local before = apr.time_now()
apr.sleep(0.25)
local after = apr.time_now()
assert(string.format('%.1f', after - before):find '^0%.[123]$')
