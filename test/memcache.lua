--[[

 Unit tests for the relational database module of the Lua/APR binding.

 Authors:
  - zhiguo zhao <zhaozg@gmail.com>
  - Peter Odding <peter@peterodding.com>
 Last Change: June 16, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

 These tests are based on http://svn.apache.org/viewvc/apr/apr/trunk/test/testmemcache.c?view=markup.
 If you have memcached running on a remote machine you can set the environment
 variable LUA_APR_MEMCACHE_HOST to the host name or IP-address of the server.
 You can also set LUA_APR_MEMCACHE_PORT to change the port number.

--]]

local status, apr = pcall(require, 'apr')
if not status then
  pcall(require, 'luarocks.require')
  apr = require 'apr'
end
local helpers = require 'apr.test.helpers'

-- You might have to change these depending on your environment.
local server_host = apr.env_get 'LUA_APR_MEMCACHE_HOST' or '127.0.0.1'
local default_port = apr.env_get 'LUA_APR_MEMCACHE_PORT' or 11211
local max_servers = 10

-- Test data.
local prefix = 'testmemcache'
local TDATA_SIZE = 500
local txt = 'Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Duis at' ..
            'lacus in ligula hendrerit consectetuer. Vestibulum tristique odio' ..
            'iaculis leo. In massa arcu, ultricies a, laoreet nec, hendrerit non,' ..
            'neque. Nulla sagittis sapien ac risus. Morbi ligula dolor, vestibulum' ..
            'nec, viverra id, placerat dapibus, arcu. Curabitur egestas feugiat' ..
            'tellus. Donec dignissim. Nunc ante. Curabitur id lorem. In mollis' ..
            'tortor sit amet eros auctor dapibus. Proin nulla sem, tristique in,' ..
            'convallis id, iaculis feugiat cras amet.'

-- Create a memcached client object.
local client = assert(apr.memcache(max_servers))
assert(apr.type(client) == 'memcache client')
assert(tostring(client):find '^memcache client %([x%x]+%)$')
local server = assert(client:add_server(server_host, default_port))
assert(client:enable_server(server))

-- Test metadata (server version and statistics).
local version, message, code = client:version(server)
if code == 'ECONNREFUSED' then
  helpers.warning("Looks like memcached isn't available! (%s)\n", message)
  return false
end
assert(type(version) == 'string')
local stats = assert(client:stats(server))
assert(type(stats) == 'table')
assert(stats.version == version)
assert(stats.pid >= 0)
assert(stats.time >= 0)
assert(stats.rusage_user >= 0)
assert(stats.rusage_system >= 0)
assert(stats.curr_items >= 0)
assert(stats.total_items >= 0)
assert(stats.bytes >= 0)
assert(stats.curr_connections >= 0)
assert(stats.total_connections >= 0)
assert(stats.connection_structures >= 0)
assert(stats.cmd_get >= 0)
assert(stats.cmd_set >= 0)
assert(stats.get_hits >= 0)
assert(stats.get_misses >= 0)
assert(stats.evictions >= 0)
assert(stats.bytes_read >= 0)
assert(stats.bytes_written >= 0)
assert(stats.limit_maxbytes >= 0)
assert(stats.threads >= 0)

-- Generate test data from the lorem ipsum text above.
local testpairs = {}
math.randomseed(os.time())
for i = 1, TDATA_SIZE do
  testpairs[prefix .. i] = txt:sub(1, math.random(1, #txt))
end

-- Test client:add(), client:replace(), client:get() and client:delete().
for key, value in pairs(testpairs) do
  -- doesn't exist yet, fail.
  assert(not client:replace(key, value))
  -- doesn't exist yet, succeed.
  assert(client:add(key, value))
  helpers.checktuple({ true, value }, assert(client:get(key)))
  -- exists now, succeed.
  assert(client:replace(key, 'new'))
  -- make sure its different.
  helpers.checktuple({ true, 'new' }, assert(client:get(key)))
  -- exists now, fail.
  assert(not client:add(key, value))
  -- clean up.
  assert(client:delete(key))
  local status, value = client:get(key)
  assert(status and not value)
end

-- Test client:incr() and client:decr().
local value = 271
assert(client:set(prefix, value))
for i = 1, TDATA_SIZE do
  local increment = math.random(1, TDATA_SIZE)
  value = value + increment
  assert(client:incr(prefix, increment) == value)
  local status, strval = assert(client:get(prefix))
  assert(tonumber(strval) == value)
  local decrement = math.random(1, value)
  value = value - decrement
  assert(client:decr(prefix, decrement) == value)
  local status, strval = assert(client:get(prefix))
  assert(tonumber(strval) == value)
end
assert(client:delete(prefix))

-- Test server management functions.
for i = 1, max_servers - 1 do
  local port = default_port + i
  local fake_server = assert(client:add_server(server_host, port))
  assert(apr.type(fake_server) == 'memcache server')
  assert(tostring(fake_server):find '^memcache server %([x%x]+%)$')
  assert(client:find_server(server_host, port))
  assert(client:enable_server(fake_server))
  local hash = client:hash(prefix)
  assert(hash > 0)
  assert(apr.type(client:find_server_hash(hash)) == 'memcache server')
  assert(client:disable_server(fake_server))
end
