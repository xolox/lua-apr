--[[

 Unit tests for the LDAP connection handling module of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: July 3, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

 The LDAP client tests need access to an LDAP server so if you want to run
 these tests you have to define the following environment variables:

  - $LUA_APR_LDAP_URL is a URL indicating SSL, host name and port
  - $LUA_APR_LDAP_WHO is the distinguished name used to bind (login)
  - $LUA_APR_LDAP_PASSWD is the password used to bind (login)
  - $LUA_APR_LDAP_BASE is the base of the directory (required to search)

--]]

local status, apr = pcall(require, 'apr')
if not status then
  pcall(require, 'luarocks.require')
  apr = require 'apr'
end
local helpers = require 'apr.test.helpers'

-- Enable overriding of test configuration using environment variables.
local SERVER = apr.env_get 'LUA_APR_LDAP_URL'
local ACCOUNT = apr.env_get 'LUA_APR_LDAP_WHO' or nil
local PASSWD = apr.env_get 'LUA_APR_LDAP_PASSWD' or nil
local BASE = apr.env_get 'LUA_APR_LDAP_BASE' or nil

-- Test apr.ldap_url_check(). {{{1
assert(apr.ldap_url_check 'ldap://root.openldap.org/dc=openldap,dc=org' == 'ldap')
assert(apr.ldap_url_check 'ldapi://root.openldap.org/dc=openldap,dc=org' == 'ldapi')
assert(apr.ldap_url_check 'ldaps://root.openldap.org/dc=openldap,dc=org' == 'ldaps')
assert(not apr.ldap_url_check 'http://root.openldap.org/dc=openldap,dc=org')

-- Test apr.ldap_url_parse(). {{{1
local url = 'ldap://root.openldap.org/dc=openldap,dc=org'
local components = assert(apr.ldap_url_parse(url))
assert(components.scheme == 'ldap')
assert(components.host == 'root.openldap.org')
assert(components.port == 389)
assert(components.scope == 'sub')
assert(components.dn == 'dc=openldap,dc=org')
assert(components.crit_exts == 0)

-- Test apr.ldap_info(). {{{1
local info, errmsg = apr.ldap_info()
if not info then
  helpers.warning "apr.ldap_info() failed, I'm guessing you don't have an LDAP library installed?\n"
  os.exit(1)
end
assert(type(info) == 'string' and info ~= '')

-- Test apr.ldap(). {{{1
local ldap_conn = assert(apr.ldap(SERVER))

-- Test tostring(ldap_conn).
assert(tostring(ldap_conn):match '^LDAP connection %([0x%x]+%)$')
assert(apr.type(ldap_conn) == 'LDAP connection')

-- Test ldap_conn:bind(). {{{1
local status, errmsg = ldap_conn:bind(ACCOUNT, PASSWD)
if not status then
  helpers.warning("Failed to bind to LDAP server, I'm assuming it's not available (reason: %s)\n", errmsg)
  os.exit(1)
end

-- Test ldap_conn:option_get() and ldap_conn:option_set(). {{{1
assert(ldap_conn:option_set('timeout', 0.5))
assert(ldap_conn:option_set('network-timeout', 0.5))
for option, typename in pairs {
  ['defbase'] = 'string',
  ['deref'] = 'number',
  ['network-timeout'] = 'number',
  ['protocol-version'] = 'number',
  ['refhop-limit'] = 'number',
  ['referrals'] = 'boolean',
  ['restart'] = 'boolean',
  ['size-limit'] = 'number',
  ['time-limit'] = 'number',
  ['timeout'] = 'number',
  ['uri'] = 'string',
} do
  local value = ldap_conn:option_get(option)
  if value ~= nil then
    assert(type(value) == typename)
    local status, errmsg = ldap_conn:option_set(option, value)
    if not status then
      -- I've made this a warning instead of an error because I'm not even sure
      -- if all options can be set and whether this goes for all SDKs.
      helpers.warning("Failed to set LDAP option %s to %s! (reason: %s)\n", option, tostring(value), errmsg)
    end
  end
end

-- Test ldap_conn:search(). {{{1
if not BASE then
  helpers.warning "Please set $LUA_APR_LDAP_BASE to enable the search tests ..\n"
else
  local attributes = {}
  local valuetypes = {}
  for dn, attrs in ldap_conn:search { scope = 'sub', base = BASE } do
    assert(attrs.objectClass, "LDAP search result without objectClass")
    for k, v in pairs(attrs) do
      local t = type(v)
      attributes[k] = (attributes[k] or 0) + 1
      valuetypes[t] = (valuetypes[t] or 0) + 1
    end
  end
  -- Assuming the search matched some entries...
  if next(valuetypes) then
    -- Check that the supported value types were tested.
    assert(valuetypes.table > 0, "No table attributes (multiple values for one attribute) in LDAP directory")
    assert(valuetypes.string > 0, "No string attributes in LDAP directory")
    assert(valuetypes.string > valuetypes.table, "Expecting more string than table attributes")
  end
  -- Again, assuming the search matched some entries...
  if next(attributes) then
    -- Check that some common attributes were found.
    assert(attributes.cn > 0, "No common names matched in LDAP directory")
    assert(attributes.sn > 0, "No last names matched in LDAP directory")
    assert(attributes.givenName > 0, "No first names matched in LDAP directory")
  end
end

-- Test ldap_conn:unbind(). {{{1
assert(ldap_conn:unbind())
