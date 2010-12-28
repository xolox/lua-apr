local apr = assert(require 'apr')
local valid_key = 'LUA_APR_MAGIC_ENV_KEY'
local invalid_key = 'LUA_APR_INVALID_ENV_KEY'
assert(apr.env_get(valid_key) == apr._VERSION)
assert(not apr.env_get(invalid_key))
apr.sleep(1)
os.exit(42)
