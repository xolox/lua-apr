--[[

 Serialization function for the Lua/APR binding.

 Last Change: November 20, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

 Based on the table-to-source serializer included in Metalua, which is
 copyright (c) 2008-2009, Fabien Fleutot <metalua@gmail.com>. Retrieved
 from https://github.com/fab13n/metalua/blob/master/src/lib/serialize.lua.

 Minor changes by Peter Odding <peter@peterodding.com> to serialize
 function upvalues and userdata objects created by the Lua/APR binding.

 Inline documentation can be found in "serialize.c" because I'm too lazy to
 change my documentation generator...

]]

local no_identity = {
  ['nil'] = true,
  ['boolean'] = true,
  ['number'] = true,
  ['string'] = true,
}

local function serialize(x)

  local gensym_max = 0  -- index of the gensym() symbol generator
  local seen_once = {} -- element->true set of elements seen exactly once in the table
  local multiple = {} -- element->varname set of elements seen more than once
  local nested = {} -- transient, set of elements currently being traversed
  local nest_points = {}
  local nest_patches = {}

  -- Generate fresh indexes to store new sub-tables:
  local function gensym()
    gensym_max = gensym_max + 1
    return gensym_max
  end

  -----------------------------------------------------------------------------
  -- `nest_points' are places where a (recursive) table appears within
  -- itself, directly or not.  for instance, all of these chunks
  -- create nest points in table `x':
  --
  -- "x = {}; x[x] = 1"
  -- "x = {}; x[1] = x"
  -- "x = {}; x[1] = { y = { x } }".
  --
  -- To handle those, two tables are created by `mark_nest_point()':
  --
  -- * `nest_points[parent]' associates all keys and values in table
  --   parent which create a nest_point with boolean `true'
  --
  -- * `nest_patches' contains a list of `{ parent, key, value }'
  --   tuples creating a nest point. They're all dumped after all the
  --   other table operations have been performed.
  --
  -- `mark_nest_point(p, k, v)' fills tables `nest_points' and
  -- `nest_patches' with information required to remember that
  -- key/value `(k,v)' creates a nest point in parent table `p'.
  -- It also marks `p' as occurring multiple times, since several
  -- references to it will be required in order to patch the nest
  -- points.
  -----------------------------------------------------------------------------
  local function mark_nest_point(parent, k, v)
    local nk, nv = nested[k], nested[v]
    assert(not nk or seen_once[k] or multiple[k])
    assert(not nv or seen_once[v] or multiple[v])
    local parent_np = nest_points[parent]
    if not parent_np then
      parent_np = {}
      nest_points[parent] = parent_np
    end
    if nk then parent_np[k] = true end
    if nv then parent_np[v] = true end
    table.insert(nest_patches, { parent, k, v })
    seen_once[parent] = nil
    multiple[parent] = true
  end

  -----------------------------------------------------------------------------
  -- 1st pass, list the tables and functions which appear more than once in `x'
  -----------------------------------------------------------------------------
  local function mark_multiple_occurrences(x)
    local t = type(x)
    if no_identity[t] then
      return
    elseif seen_once[x] then
      seen_once[x] = nil
      multiple[x] = true
    elseif not multiple[x] then
      seen_once[x] = true
    end
    if t == 'table' then
      nested[x] = true
      for k, v in pairs(x) do
        if nested[k] or nested[v] then
          mark_nest_point(x, k, v)
        else
          mark_multiple_occurrences(k)
          mark_multiple_occurrences(v)
        end
      end
      nested[x] = nil
    elseif t == 'function' then
      for i = 1, math.huge do
        local n, v = debug.getupvalue(x, i)
        if n then mark_multiple_occurrences(v) else break end
      end
    end
  end

  local dumped = {} -- multiply occurring values already dumped in localdefs
  local localdefs = {} -- already dumped local definitions as source code lines

  -- mutually recursive functions:
  local dump_val, dump_or_ref_val

  ------------------------------------------------------------------------------
  -- if `x' occurs multiple times, dump the local var rather than the
  -- value. If it's the first time it's dumped, also dump the content
  -- in localdefs.
  ------------------------------------------------------------------------------
  function dump_or_ref_val(x)
    if nested[x] then
      -- placeholder for recursive reference
      return 'false'
    elseif not multiple[x] then
      -- value referenced only once, dump directly
      return dump_val(x)
    else
      local var = dumped[x]
      if var then
        -- already referenced
        return '_[' .. var .. ']'
      else
        -- first occutrence, create and register reference
        local val = dump_val(x)
        var = gensym()
        table.insert(localdefs, '_[' .. var .. '] = ' .. val)
        dumped[x] = var
        return '_[' .. var .. ']'
      end
    end
  end

  -----------------------------------------------------------------------------
  -- 2nd pass, dump the object; subparts occurring multiple times are dumped
  -- in local variables, which can then be referenced multiple times;
  -- care is taken to dump local vars in an order which respect dependencies.
  -----------------------------------------------------------------------------
  function dump_val(x)
    local t = type(x)
    if x == nil then
      return 'nil'
    elseif t == 'number' then
      return x == math.huge and 'math.huge' or string.format('%.16f', x)
    elseif t == 'string' then
      return string.format('%q', x)
    elseif t == 'boolean' then
      return x and 'true' or 'false'
    elseif t == 'function' then
      local body = string.format("loadstring(%q, '@serialized')", string.dump(x))
      if not debug.getupvalue(x, 1) then return body end
      local acc = {}
      -- FIXME This doesn't actually need an anonymous function, I'm just lazy.
      table.insert(acc, '(function()')
      table.insert(acc, 'local f = ' .. body)
      for i = 1, math.huge do
        local n, v = debug.getupvalue(x, i)
        if not n then break end
        table.insert(acc, string.format('debug.setupvalue(f, %i, %s)', i, dump_or_ref_val(v)))
      end
      table.insert(acc, 'return f')
      table.insert(acc, 'end)()')
      return table.concat(acc, '\n')
    elseif t == 'table' then
      local acc = {}
      local idx_dumped = {}
      local np = nest_points[x]
      for i, v in ipairs(x) do
        if np and np[v] then
          table.insert(acc, 'false') -- placeholder
        else
          table.insert(acc, dump_or_ref_val(v))
        end
        idx_dumped[i] = true
      end
      for k, v in pairs(x) do
        if np and (np[k] or np[v]) then
          --check_multiple(k); check_multiple(v) -- force dumps in localdefs
        elseif not idx_dumped[k] then
          table.insert(acc, '[' .. dump_or_ref_val(k) .. '] = ' .. dump_or_ref_val(v))
        end
      end
      return '{ ' .. table.concat(acc, ', ') .. ' }'
    elseif t == 'userdata' then
      local apr = require 'apr'
      return string.format("require('apr').deref(%q)", apr.ref(x))
    else
      error("Can't serialize data of type " .. t)
    end
  end

  -- Patch the recursive table entries:
  local function dump_nest_patches()
    for _, entry in ipairs(nest_patches) do
      local p, k, v = unpack(entry)
      assert(multiple[p])
      table.insert(localdefs, dump_or_ref_val(p)
          .. '[' .. dump_or_ref_val(k) .. '] = '
          .. dump_or_ref_val(v) .. ' -- rec')
    end
  end

  mark_multiple_occurrences(x)
  local toplevel = dump_or_ref_val(x)
  dump_nest_patches()

  if next(localdefs) then
    -- Dump local vars containing shared or recursive parts,
    -- then the main table using them.
    return 'local _={}\n' ..
      table.concat(localdefs, '\n') ..
      '\nreturn ' .. toplevel
  else
    -- No shared part, straightforward dump:
    return 'return ' .. toplevel
  end

end

return serialize

-- vim: ts=2 sw=2 et
