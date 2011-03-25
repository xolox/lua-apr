--[[

 Unit tests for the date parsing module of the Lua/APR binding.

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

-- The following tests were copied from http://svn.apache.org/viewvc/apr/apr/trunk/test/testdate.c?view=markup
for i, rfc_pair in ipairs {
  { "Mon, 27 Feb 1995 20:49:44 -0800",  "Tue, 28 Feb 1995 04:49:44 GMT" },
  { "Fri,  1 Jul 2005 11:34:25 -0400",  "Fri, 01 Jul 2005 15:34:25 GMT" },
  { "Monday, 27-Feb-95 20:49:44 -0800", "Tue, 28 Feb 1995 04:49:44 GMT" },
  { "Tue, 4 Mar 1997 12:43:52 +0200",   "Tue, 04 Mar 1997 10:43:52 GMT" },
  { "Mon, 27 Feb 95 20:49:44 -0800",    "Tue, 28 Feb 1995 04:49:44 GMT" },
  { "Tue,  4 Mar 97 12:43:52 +0200",    "Tue, 04 Mar 1997 10:43:52 GMT" },
  { "Tue, 4 Mar 97 12:43:52 +0200",     "Tue, 04 Mar 1997 10:43:52 GMT" },
  { "Mon, 27 Feb 95 20:49 GMT",         "Mon, 27 Feb 1995 20:49:00 GMT" },
  { "Tue, 4 Mar 97 12:43 GMT",          "Tue, 04 Mar 1997 12:43:00 GMT" }
} do
  local time = assert(apr.date_parse_rfc(rfc_pair[1]))
  assert(apr.time_format('rfc822', time) == rfc_pair[2])
end

-- The tests for apr_date_parse_http() are far too complex to reproduce in Lua
-- so instead here's a round trip of one of the examples in the documentation:
local date = 'Sun, 06 Nov 1994 08:49:37 GMT'
assert(apr.time_format('rfc822', apr.date_parse_http(date)) == date)
