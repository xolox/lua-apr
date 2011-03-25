--[[

 Unit tests for the HTTP request parsing module of the Lua/APR binding.

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
if not apr.parse_headers then
  helpers.warning "HTTP request parsing module not available!\n"
  return false
end

local function nl2crlf(s)
  return (s:gsub('\r?\n', '\r\n'))
end

-- Test apr.parse_query_string() {{{1
local expected = { alpha = 'one', beta = 'two', omega = 'last' }
local actual = assert(apr.parse_query_string 'alpha=one&beta=two;omega=last')
assert(helpers.deepequal(expected, actual))
local expected = { key = { '1', '2', '3' }, novalue = true }
local actual = assert(apr.parse_query_string 'key=1;key=2;key=3;novalue')
assert(helpers.deepequal(expected, actual))

-- Test apr.parse_cookie_header() {{{1

-- Netscape cookies
local nscookies = 'a=1; foo=bar; fl=left; fr=right;bad; ns=foo=1&bar=2,frl=right-left; flr=left-right; fll=left-left; good_one=1;=;bad'
local cookies, msg, code = assert(apr.parse_cookie_header(nscookies))
assert(code == 'NOTOKEN')
assert(cookies.bad == nil) -- ignore wacky cookies that don't have an '=' sign
assert(helpers.deepequal({
  a = '1',
  ns = 'foo=1&bar=2', -- accept wacky cookies that contain multiple '='
  foo = 'bar',
  fl = 'left',
  fr = 'right',
  frl = 'right-left',
  flr = 'left-right',
  fll = 'left-left',
  good_one = '1',
}, cookies))

-- RFC cookies
local rfccookies = '$Version=1; first=a;$domain=quux;second=be,$Version=1;third=cie'
local cookies, msg, code = assert(apr.parse_cookie_header(rfccookies))
assert(not (msg or code))
assert(helpers.deepequal({
  first = 'a',
  second = 'be',
  third = 'cie',
}, cookies))

-- Invalid cookies.
local wpcookies = 'wordpressuser_c580712eb86cad2660b3601ac04202b2=admin; wordpresspass_c580712eb86cad2660b3601ac04202b2=7ebeeed42ef50720940f5b8db2f9db49; rs_session=59ae9b8b503e3af7d17b97e7f77f7ea5; dbx-postmeta=grabit=0-,1-,2-,3-,4-,5-,6-&advancedstuff=0-,1+,2-'
local cookies, msg, code = apr.parse_cookie_header(wpcookies)
assert(code == 'NOTOKEN')

local cgcookies1 = 'UID=MTj9S8CoAzMAAFEq21YAAAAG|c85a9e59db92b261408eb7539ff7f949b92c7d58; $Version=0;SID=MTj9S8CoAzMAAFEq21YAAAAG|c85a9e59db92b261408eb7539ff7f949b92c7d58;$Domain=www.xxxx.com;$Path=/'
local cookies, msg, code = apr.parse_cookie_header(cgcookies1)
assert(code == 'MISMATCH')

local cgcookies2 = 'UID=Gh9VxX8AAAIAAHP7h6AAAAAC|2e809a9cc99c2dca778c385ebdefc5cb86c95dc3; SID=Gh9VxX8AAAIAAHP7h6AAAAAC|2e809a9cc99c2dca778c385ebdefc5cb86c95dc3; $Version=1'
local cookies, msg, code = apr.parse_cookie_header(cgcookies2)
assert(code == 'MISMATCH')

local cgcookies3 = 'UID=hCijN8CoAzMAAGVDO2QAAAAF|50299f079343fd6146257c105b1370f2da78246a; SID=hCijN8CoAzMAAGVDO2QAAAAF|50299f079343fd6146257c105b1370f2da78246a; $Path="/"; $Domain="www.xxxx.com"'
local cookies, msg, code = apr.parse_cookie_header(cgcookies3)
assert(code == 'MISMATCH')

-- Valid cookie.
local cgcookies4 = 'SID=66XUEH8AAAIAAFmLLRkAAAAV|2a48c4ae2e9fb8355e75192db211f0779bdce244; UID=66XUEH8AAAIAAFmLLRkAAAAV|2a48c4ae2e9fb8355e75192db211f0779bdce244; __utma=144149162.4479471199095321000.1234471650.1234471650.1234471650.1; __utmb=144149162.24.10.1234471650; __utmc=144149162; __utmz="144149162.1234471650.1.1.utmcsr=szukaj.xxxx.pl|utmccn=(referral)|utmcmd=referral|utmcct=/internet/0,0.html"'
local cookies, msg, code = apr.parse_cookie_header(cgcookies4)
assert(type(cookies) == 'table')
assert(not (msg or code))

-- Test apr.parse_headers() {{{1

local expected = {
  ['Host'] = 'lua-users.org',
  ['User-Agent'] = 'Mozilla/5.0 (X11; U; Linux i686; nl; rv:1.9.2.13) Gecko/20101206 Ubuntu/10.04 (lucid) Firefox/3.6.13',
  ['Accept'] = 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
  ['Accept-Language'] = 'nl,en-us;q=0.7,en;q=0.3',
  ['Accept-Encoding'] = 'gzip,deflate',
  ['Accept-Charset'] = 'ISO-8859-1,utf-8;q=0.7,*;q=0.7',
  ['Keep-Alive'] = '115',
  ['Connection'] = 'keep-alive',
  ['Cookie'] = 'wiki=randkey&944319634&rev&1&id&4216',
}

local actual = assert(apr.parse_headers(nl2crlf [[
Host: lua-users.org
User-Agent: Mozilla/5.0 (X11; U; Linux i686; nl; rv:1.9.2.13) Gecko/20101206 Ubuntu/10.04 (lucid) Firefox/3.6.13
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8
Accept-Language: nl,en-us;q=0.7,en;q=0.3
Accept-Encoding: gzip,deflate
Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7
Keep-Alive: 115
Connection: keep-alive
Cookie: wiki=randkey&944319634&rev&1&id&4216

]]))

assert(helpers.deepequal(expected, actual))

-- Test apr.parse_multipart() {{{1

local expected = {
 field1 = {
  'Joe owes =80100.',
  ['content-disposition'] = 'form-data; name="field1"',
  ['content-transfer-encoding'] = 'quoted-printable',
  ['content-type'] = 'text/plain;charset=windows-1250'
 },
 pics = {
  'file1.txt',
  '... contents of file1.txt ...\r\n',
  ['Content-Type'] = 'text/plain',
  ['content-disposition'] = 'form-data; name="pics"; filename="file1.txt"'
 },
 [''] = {
  'Joe owes =80100.',
  ['content-disposition'] = 'form-data; name=""',
  ['content-transfer-encoding'] = 'quoted-printable',
  ['content-type'] = 'text/plain;\r\n charset=windows-1250'
 }
}

local actual = assert(apr.parse_multipart(nl2crlf [[
--AaB03x
content-disposition: form-data; name="field1"
content-type: text/plain;charset=windows-1250
content-transfer-encoding: quoted-printable

Joe owes =80100.
--AaB03x
content-disposition: form-data; name="pics"; filename="file1.txt"
Content-Type: text/plain

... contents of file1.txt ...

--AaB03x
content-disposition: form-data; name=""
content-type: text/plain;
 charset=windows-1250
content-transfer-encoding: quoted-printable

Joe owes =80100.
--AaB03x--
]], 'multipart/form-data; charset="iso-8859-1"; boundary="AaB03x"'))

assert(helpers.deepequal(expected, actual))

-- Test apr.header_attribute() {{{1

local header = 'text/plain; boundary="-foo-", charset=ISO-8859-1';
local value, msg, code = apr.header_attribute(header, 'none')
assert(code == 'NOATTR')
local value, msg, code = apr.header_attribute(header, 'set')
assert(code == 'NOATTR')
assert(apr.header_attribute(header, 'boundary') == '-foo-')
assert(apr.header_attribute(header, 'charset') == 'ISO-8859-1')

local header = 'max-age=20; no-quote="...'
assert(apr.header_attribute(header, 'max-age') == '20')
local value, msg, code = apr.header_attribute(header, 'age')
assert(code == 'BADSEQ')
local value, msg, code = apr.header_attribute(header, 'no-quote')
assert(code == 'BADSEQ')

-- Test apr.uri_encode() and apr.uri_decode() {{{1

local reserved = {
  ['!'] = '%21', ['*'] = '%2A', ["'"] = '%27', ['('] = '%28', [')'] = '%29',
  [';'] = '%3B', [':'] = '%3A', ['@'] = '%40', ['&'] = '%26', ['='] = '%3D',
  ['+'] = '%2B', ['$'] = '%24', [','] = '%2C', ['/'] = '%2F', ['?'] = '%3F',
  ['#'] = '%23', ['['] = '%5B', [']'] = '%5D'
}

for plain, encoded in pairs(reserved) do
  assert(apr.uri_encode(plain) == encoded)
  assert(apr.uri_encode(plain) == encoded)
end
