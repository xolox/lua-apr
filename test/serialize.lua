--[[

 Tests for the serialization function of the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: November 20, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

]]

local status, apr = pcall(require, 'apr')
if not status then
  pcall(require, 'luarocks.require')
  apr = require 'apr'
end

function main()

  -- Test single, scalar values. {{{1
  assert(roundtrip(nil), "Failed to serialize nil value")
  assert(roundtrip(true), "Failed to serialize true value")
  assert(roundtrip(false), "Failed to serialize false value")
  assert(roundtrip(0), "Failed to serialize number value (0)")
  assert(roundtrip(1), "Failed to serialize number value (1)")
  assert(roundtrip(-1), "Failed to serialize number value (-1)")
  assert(roundtrip(math.pi), "Failed to serialize number value (math.pi)")
  assert(roundtrip(math.huge), "Failed to serialize number value (math.huge)")
  assert(roundtrip(''), "Failed to serialize empty string")
  assert(roundtrip('simple'), "Failed to serialize simple string ('simple')")
  assert(roundtrip('foo\nbar\rbaz\0qux'), "Failed to serialize complex string ('foo\\nbar\\rbaz\\0qux')")

  -- Test multiple scalar values (a tuple). {{{1
  assert(roundtrip(true, false, nil, 13), "Failed to serialize tuple with nil value")

  -- Test tables (empty, list, dictionary, nested). {{{1
  assert(roundtrip({}), "Failed to serialize empty table")
  assert(roundtrip({1, 2, 3, 4, 5}), "Failed to serialize list like table ({1, 2, 3, 4, 5})")
  assert(roundtrip({pi=math.pi}), "Failed to serialize table ({pi=3.14})")
  assert(roundtrip({nested={42}}), "Failed to serialize nested table ({nested={42}})")

  -- Test tables with cycles. {{{1
  local a, b = {}, {}; a.b, b.a = b, a
  local chunk = apr.serialize(a, b)
  local a2, b2 = apr.unserialize(chunk)
  assert(a2.b == b2 and b2.a == a2)

  -- Test simple Lua function. {{{1
  assert(roundtrip(function() return 42 end), "Failed to serialize simple function (return 42)")

  -- Test Lua function with scalar upvalue. {{{1
  local simple_upvalue = 42
  assert(roundtrip(function() return simple_upvalue end), "Failed to serialize function with scalar upvalue")

  -- Test Lua function with multiple upvalues. {{{1
  local a, b, c, d = 1, 9, 8, 6
  assert(roundtrip(function() return a, b, c, d end), "Failed to serialize function with multiple upvalues")

  -- Test Lua function with complex upvalues. {{{1
  local nested = {1, 2, 3, 4, 5}
  local complex_upvalue = {pi=math.pi, string=name_of_upvalue, nested=nested}
  assert(roundtrip(function() return nested, complex_upvalue end), "Failed to serialize function with complex upvalues")

  -- Test Lua/APR userdata. {{{1
  local object = apr.pipe_open_stdin()
  local data = apr.serialize(object)
  local result = apr.unserialize(data)
  assert(object == result, "Failed to preserve userdata identity!")

end

function pack(...)
  return { n = select('#', ...), ... }
end

function roundtrip(...)
  return deepequals(pack(...), pack(apr.unserialize(apr.serialize(...))))
end

function deepequals(a, b)
  if a == b then return true end
  local at, bt = type(a), type(b)
  if at ~= bt then return false end
  if at == 'function' then
    -- Compare functions based on return values.
    return deepequals(pack(a()), pack(b()))
  end
  if at ~= 'table' then
    -- Everything except functions and tables can be compared literally.
    return a == b
  end
  -- Compare the two tables by iterating the keys of both tables.
  for k, v in pairs(a) do
    if not deepequals(v, b[k]) then
      return false
    end
  end
  for k, v in pairs(b) do
    if not deepequals(v, a[k]) then
      return false
    end
  end
  return true
end

main()
