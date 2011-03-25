--[[

 Unit tests for the cryptography module of the Lua/APR binding.

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

-- Sample data from http://en.wikipedia.org/wiki/MD5#MD5_hashes
-- and http://en.wikipedia.org/wiki/SHA1#Example_hashes

-- Check that MD5 hashing returns the expected result.
assert(apr.md5 '' == 'd41d8cd98f00b204e9800998ecf8427e')
assert(apr.md5 'The quick brown fox jumps over the lazy dog' == '9e107d9d372bb6826bd81d3542a419d6')
assert(apr.md5 'The quick brown fox jumps over the lazy eog' == 'ffd93f16876049265fbaef4da268dd0e')

local pass, salt = 'password', 'salt'
hash = apr.md5_encode(pass, salt)
-- Test that MD5 passwords can be validated.
assert(apr.password_validate(pass, hash))

-- Test that SHA1 hashing returns the expected result.
assert(apr.sha1 'The quick brown fox jumps over the lazy dog' == '2fd4e1c67a2d28fced849ee1bb76e7391b93eb12')
assert(apr.sha1 'The quick brown fox jumps over the lazy cog' == 'de9f2c7fd25e1b3afad3e85a0bd17d9b100db4b3')

-- Test the incremental MD5 message digest interface.
local context = assert(apr.md5_init())
assert(context:update 'The quick brown fox jumps over the lazy dog')
assert(context:digest() == '9e107d9d372bb6826bd81d3542a419d6')
assert(context:reset())
assert(context:update 'The quick brown fox jumps over the lazy eog')
assert(context:digest() == 'ffd93f16876049265fbaef4da268dd0e')

-- Test the incremental SHA1 message digest interface.
local context = assert(apr.sha1_init())
assert(context:update 'The quick brown fox jumps over the lazy dog')
assert(context:digest() == '2fd4e1c67a2d28fced849ee1bb76e7391b93eb12')
assert(context:reset())
assert(context:update 'The quick brown fox jumps over the lazy cog')
assert(context:digest() == 'de9f2c7fd25e1b3afad3e85a0bd17d9b100db4b3')
