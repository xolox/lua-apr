--[[

 Unit tests for the XML parsing module of the Lua/APR binding.

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

local function parse_xml(text)
  local parser = assert(apr.xml())
  assert(parser:feed(text))
  assert(parser:done())
  local info = assert(parser:getinfo())
  assert(parser:close())
  return info
end

-- Empty root element.
assert(helpers.deepequal(parse_xml '<elem/>',
    { tag = 'elem' }))

-- Element with a single attribute.
assert(helpers.deepequal(parse_xml '<elem name="value" />',
    { tag = 'elem', attr = { 'name', name = 'value' } }))

-- Element with multiple attributes.
assert(helpers.deepequal(parse_xml '<elem name="value" a2="2" a3="3" />',
    { tag = 'elem', attr = { 'name', 'a2', 'a3', name = 'value', a2 = '2', a3 = '3' }}))

-- Element with text child node.
assert(helpers.deepequal(parse_xml '<elem>text</elem>',
    { tag = 'elem', 'text' }))

-- Element with child element.
assert(helpers.deepequal(parse_xml '<elem><child/></elem>',
    { tag = 'elem', { tag = 'child' } }))

-- Element with child element that contains a text node.
assert(helpers.deepequal(parse_xml '<parent><child>text</child></parent>',
    { tag = 'parent', { tag = 'child', 'text' } }))

-- Create a temporary XML file to test the alternative constructor.
local xml_path = helpers.tmpname()
helpers.writefile(xml_path, '<parent><child>text</child></parent>')
local parser = assert(apr.xml(xml_path))
assert(helpers.deepequal(parser:getinfo(),
    { tag = 'parent', { tag = 'child', 'text' } }))

-- Check that tostring(apr.xml()) works as expected.
local parser = assert(apr.xml())
assert(tostring(parser):find '^xml parser %([x%x]+%)$')
assert(parser:close())
assert(tostring(parser):find '^xml parser %(closed%)$')
