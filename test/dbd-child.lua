-- Based on http://svn.apache.org/viewvc/apr/apr/trunk/test/testdbd.c?view=markup

local status, apr = pcall(require, 'apr')
if not status then
  pcall(require, 'luarocks.require')
  apr = require 'apr'
end

local helpers = require 'apr.test.helpers'
local driver = assert(apr.dbd 'sqlite3')
assert(driver:open ':memory:')
assert(driver:driver() == 'sqlite3')

-- Check that the "connection" is alive.
assert(driver:check())

-- The SQLite driver uses a single database.
assert(not driver:dbname 'anything')

-- Helpers {{{1

function check_label(object, pattern)
  local label = assert(tostring(object))
  assert(label:find(pattern))
end

check_label(driver, '^database driver %([x%x]+%)$')

local function delete_rows()
  local status, affected_rows = assert(driver:query 'DELETE FROM apr_dbd_test')
  assert(affected_rows == 26)
end

local function count_rows()
  local results = assert(driver:select 'SELECT COUNT (*) FROM apr_dbd_test')
  return results:tuple() + 0
end

-- Create table. {{{1
assert(driver:query [[
  CREATE TABLE apr_dbd_test (
    col1 varchar(40) not null,
    col2 varchar(40),
    col3 integer)
]])

-- Insert rows using prepared statement. {{{1
local prepared_statement = assert(driver:prepare [[
  INSERT INTO apr_dbd_test VALUES (%s, %s, %i)
]])
check_label(prepared_statement, '^prepared statement %([x%x]+%)$')
for i = string.byte 'a', string.byte 'z' do
  local c = string.char(i)
  local status, nrows
  -- Test both statement:query() interfaces.
  if i % 2 == 0 then
    status, nrows = assert(prepared_statement:query { c, i, i })
  else
    status, nrows = assert(prepared_statement:query(c, i, i))
  end
  assert(status and nrows == 1)
end

-- Test transactions and rolling back. {{{1
assert(driver:transaction_start())
assert(driver:transaction_mode 'rollback')
assert(count_rows() == 26)
delete_rows()
assert(count_rows() == 0)
assert(driver:transaction_end())
assert(count_rows() == 26)

-- Make sure prepared_statement:query() rejects invalid values correctly. {{{1
local prepared_statement = assert(driver:prepare 'INSERT INTO apr_dbd_test VALUES (%s, %s, %i)')
local status, message = pcall(function() prepared_statement:query('', 1, {}) end)
assert(not status)
assert(message:find 'bad argument #3')
local status, message = pcall(function() prepared_statement:query { '', {}, 1 } end)
assert(not status)
assert(message:find 'bad argument #1')
assert(message:find 'invalid value at index 2')

-- Test resultset:tuples() iterator. {{{1
local results = assert(driver:select [[
  SELECT col1, col2, col3
  FROM apr_dbd_test
  ORDER BY col1
]])
check_label(results, '^result set %([x%x]+%)$')
local expected = string.byte 'a'
for col1, col2, col3 in results:tuples() do
  assert(col1 == string.char(expected))
  assert(tonumber(col2) == expected)
  assert(tonumber(col3) == expected)
  expected = expected + 1
end
assert(expected == (string.byte 'z' + 1))

-- Test resultset:tuple() function. {{{1

-- Tuple containing one number. {{{2
local results = assert(driver:select 'SELECT 42')
-- Check the number of results.
assert(#results == 1)
-- Check the resulting tuple.
helpers.checktuple({ '42' }, results:tuple(1))
-- Check that just one tuple is returned.
assert(not results:tuple())

-- Tuple containing two numbers. {{{2
local results = assert(driver:select 'SELECT 42, 24')
assert(#results == 1)
helpers.checktuple({ '42', '24' }, results:tuple(1))
assert(not results:tuple())

-- Tuple containing number and string. {{{2
local results = assert(driver:select [[ SELECT 42, 'hello world!' ]])
assert(#results == 1)
helpers.checktuple({ '42', 'hello world!' }, results:tuple(1))
assert(not results:tuple())

-- Named tuple containing number and string. {{{2
local results = assert(driver:select [[ SELECT 42 AS "num", 'hello world!' AS "str" ]])
assert(#results == 1)
helpers.checktuple({ 'num', 'str' }, results:columns())
assert(not results:columns(0))
assert(results:columns(1) == 'num')
assert(results:columns(2) == 'str')
assert(not results:columns(3))
helpers.checktuple({ '42', 'hello world!' }, results:tuple(1))
assert(not results:tuple())

-- Result set with multiple tuples. {{{2
local results = assert(driver:select [[
  SELECT col1, col2, col3
  FROM apr_dbd_test
  ORDER BY col1
]])
local expected = string.byte 'a'
helpers.checktuple({ 'col1', 'col2', 'col3' }, results:columns())
while true do
  local col1, col2, col3 = results:tuple()
  if not col1 then break end
  assert(col1 == string.char(expected))
  assert(tonumber(col2) == expected)
  assert(tonumber(col3) == expected)
  expected = expected + 1
end
assert(expected == (string.byte 'z' + 1))

-- Test resultset:rows() iterator. {{{1
local results = assert(driver:select [[
  SELECT col1, col2, col3
  FROM apr_dbd_test
  ORDER BY col1
]])
local expected = string.byte 'a'
for row in results:rows() do
  assert(helpers.deepequal({
    col1 = string.char(expected),
    col2 = tostring(expected),
    col3 = tostring(expected)
  }, row))
  expected = expected + 1
end
assert(expected == (string.byte 'z' + 1))

-- Test resultset:row() function. {{{1

-- Basic usage.
local results = assert(driver:select [[ SELECT 42 AS "num", 'hello world!' AS "str" ]])
assert(#results == 1)
assert(helpers.deepequal({ num = '42', str = 'hello world!' }, results:row(1)))
assert(not results:row())

-- NULL translates to nil.
local results = assert(driver:select [[ SELECT 42 AS "num", 'hello world!' AS "str", NULL AS "whatever" ]])
assert(#results == 1)
assert(helpers.deepequal({ num = '42', str = 'hello world!' }, results:row(1)))
assert(not results:row())

-- True and false translate to 0 and 1.
local results = assert(driver:select [[ SELECT (1 == 1) AS "t", (1 == 2) AS "f" ]])
assert(#results == 1)
assert(helpers.deepequal({ t = '1', f = '0' }, results:row(1)))

-- Test resultset:pairs() iterator. {{{1
local results = assert(driver:select [[
  SELECT col1, col2, col3
  FROM apr_dbd_test
  ORDER BY col1
]])
local ii = 1
local expected = string.byte 'a'
for i, row in results:pairs() do
  assert(helpers.deepequal({
    col1 = string.char(expected),
    col2 = tostring(expected),
    col3 = tostring(expected)
  }, row))
  assert(i == ii)
  expected = expected + 1
  ii = ii + 1
end
assert(expected == (string.byte 'z' + 1))

-- Delete all rows in table. {{{1
delete_rows()

-- Drop table. {{{1
assert(driver:query [[
  DROP TABLE apr_dbd_test
]])

-- Make sure prepared statements & result sets are invalidated when driver is reinitialized. {{{1

local function check_reinitialized(cb)
  local status, message = pcall(cb)
  assert(not status)
  assert(message:find 'database driver has been reinitialized')
end

local results = driver:select 'SELECT 1, 2, 3'
assert(apr.type(results) == 'result set')
helpers.checktuple({ '1', '2', '3' }, results:tuple(1))

local statement = driver:prepare 'SELECT 1, 2, 3'
assert(apr.type(statement) == 'prepared statement')
local statement_results = assert(statement:select())
assert(apr.type(statement_results) == 'result set')

driver:close()
check_reinitialized(function() results:row() end)
check_reinitialized(function() statement:select() end)
