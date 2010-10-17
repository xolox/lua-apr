--[[

 Documentation generator for the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: October 18, 2010
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

 This Lua script uses the 

]]

local SOURCES = [[ base64.c crypt.c env.c filepath.c fnmatch.c io_dir.c
  io_file.c io_pipe.c lua_apr.c permissions.c proc.c stat.c str.c time.c uri.c
  user.c uuid.c apr.lua ]]

local modules = {}
local misc_module = {functions={}}

local function getmodule(name, file, header)
  if not name then
    return misc_module
  else
    local key = name:lower()
    local value = modules[key]
    if not value then
      value = {name=name, file=file, header=header, functions={}}
      modules[key] = value
    end
    return value
  end
end

local function message(string, ...)
  io.stderr:write(string:format(...), '\n')
end

local function trim(s)
  return s:match '^%s*(.-)%s*$'
end

local function stripfoldmarker(s)
  return trim(s:gsub('{{%{%d', ''))
end

local function stripcomment(s)
  return trim(s:gsub('\n %* ?', '\n'))
end

for filename in SOURCES:gmatch '%S+' do
  local handle = assert(io.open('src/' .. filename))
  local source = assert(handle:read('*all')):gsub('\r\n', '\n')
  if filename:find '%.c$' then
    -- Don't extract documentation from ignored C source code.
    source = source:gsub('\n#if 0.-\n#endif', '')
  end
  handle:close()
  local header = stripcomment(source:match '^/%*(.-)%*/' or '')
  local modulename = header:match '^([^\n]-) module'
  if not modulename then
    message("%s: Failed to determine module name!", filename)
  end
  local module = getmodule(modulename, filename, header)
  if filename:find '%.c$' then
    local pattern = '\n/%*(.-)%*/\n\n%w[^\n]- ([%w_]+)%([^\n]-%)\n{'
    for docblock, funcname in source:gmatch(pattern) do
      local signature, description = docblock:match '^([^\n]- %-> [^\n]+)(\n.-)$'
      if not signature then
        message("%s: Function %s doesn't have a signature!", filename, funcname)
      else
        table.insert(module.functions, {
          signature = stripfoldmarker(signature),
          description = stripcomment(description) })
      end
    end
  else
    local pattern = '\n(\n%-%- apr%.[%w_]+%(.-\n%-%-[^\n]*)\n\nfunction (apr%.[%w_]+)%('
    for docblock, funcname in source:gmatch(pattern) do
      docblock = trim(docblock:gsub('\n%-%- ?', '\n'))
      local signature, description = docblock:match '^([^\n]- %-> [^\n]+)(\n.-)$'
      if not signature then
        message("%s: Function %s doesn't have a signature!", filename, funcname)
      else
        local text, modulename = description:match '^(.-)\nPart of the "(.-)" module%.$'
        local module = getmodule(modulename, filename)
        table.insert(module.functions, {
          signature = stripfoldmarker(signature), -- strip Vim fold markers
          description = text or description })
      end
    end
  end
end

local sorted_modules = {}
for _, module in pairs(modules) do
  table.insert(sorted_modules, module)
end
table.sort(sorted_modules, function(a, b)
  return a.file < b.file
end)

local blocks = { trim [[

# Lua/APR binding documentation

The [Lua/APR binding] [homepage] aims to bring most of the functionality in the
[Apache Portable Runtime (APR)] [apr_wiki] to the small and flexible
[programming language Lua] [lua_wiki]. This document contains the documentation
for the Lua/APR binding. Some notes about this documentation:

 - Most of the documentation is based on the [official APR documentation]
   [apr_docs] but I've had to consult the APR source code here and there when
   the official documentation didn't suffice. If anything seems amiss [please
   let me know](mailto:peter@peterodding.com).

 - The entries for all functions are automatically generated from the C and Lua
   source code of the binding to prevent that documentation and implementation
   become out of sync.

[homepage]: http://peterodding.com/code/lua/apr/
[apr_wiki]: http://en.wikipedia.org/wiki/Apache_Portable_Runtime
[lua_wiki]: http://en.wikipedia.org/wiki/Lua_%28programming_language%29
[apr_docs]: http://apr.apache.org/docs/apr/trunk/modules.html

]] }

function blocks:add(string, ...)
  self[#self+1] = string:format(...)
end

local function toanchor(s)
  return s:lower():gsub('[^a-z0-9_.:]+', '_')
end

local function preprocess(s)
  return (s
    :gsub('(%s)@([%w_]+)', '%1`%2`')
    :gsub('([%s])true([%s%p])', '%1<tt>*true*</tt>%2')
    :gsub('([%s%p])false([%s%p])', '%1<tt>*false*</tt>%2')
    :gsub('([%s%p])nil([%s%p])', '%1<tt>*nil*</tt>%2')
    :gsub('`([%w_.]+)%(%)`', function(funcname)
      local target
      if funcname:find '^apr%.[%w_]+$' or funcname:find '^[%w_]+:[%w_]+$' then
        target = '#' .. toanchor(funcname)
      else
        target = 'http://www.lua.org/manual/5.1/manual.html#pdf-' .. funcname
      end
      return ('<a href="%s">%s()</a>'):format(target, funcname)
    end))
end

blocks:add '## List of modules'

local items = {}
for _, module in ipairs(sorted_modules) do
  if next(module.functions) then
    items[#items + 1] = (' - [%s](#%s)'):format(module.name, toanchor(module.name))
  end
end

blocks:add('%s', table.concat(items, '\n'))

local function dumpentries(functions)
  for _, entry in ipairs(functions) do
    local signature = entry.signature:gsub('%->', '→')
    local funcname = signature:match '^[%w_.:]+'
    if funcname then
      local anchor = toanchor(funcname)
      blocks:add('### <a name="%s" href="#%s">`%s`</a>', anchor, anchor, signature)
    else
      blocks:add('### `%s`', signature)
    end
    blocks:add('%s', preprocess(entry.description))
  end
end

for _, module in ipairs(sorted_modules) do
  if next(module.functions) then
    local a = toanchor(module.name)
    blocks:add('## <a name="%s" href="#%s">%s</a>', a, a, module.name)
    local intro = module.header
    if intro then
      intro = intro:match '\n\n.-\n\n(.-)$'
      if (intro or ''):find '%S' then
        blocks:add('%s', preprocess(intro))
      end
    end
    dumpentries(module.functions)
  end
end

if next(misc_module.functions) then
  blocks:add('## Miscellaneous functions')
  dumpentries(misc_module.functions)
end

-- Join the blocks of Markdown source and write them to the 1st file.
local mkd_source = table.concat(blocks, '\n\n')
local output = assert(io.open(arg[1], 'w'))
assert(output:write(mkd_source))
assert(output:close())

-- Generate and write the HTML output to the 2nd file. This isn't actually used
-- on http://peterodding.com/code/lua/apr/docs/ but does enable immediate
-- feedback when updating the documentation.
local output = assert(io.open(arg[2], 'w'))
assert(output:write([[
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN"
"http://www.w3.org/TR/html4/strict.dtd">
<html>
 <head>
  <title>Lua/APR binding documentation</title>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
  <style type="text/css">
    body { font-family: sans-serif; padding: 1em 30% 10em 1em; cursor: default; }
    #breadcrumbs, .anchor:link, .anchor:visited { color: #EEE; }
    a:link, a:visited, #breadcrumbs a:link, #breadcrumbs a:visited, .anchor:hover, h2 a:link, h2 a:visited, h3 a:link, h3 a:visited { color: #000080; }
    a:hover, a:active, .anchor:active { color: #F00; }
    pre, code, tt { font-family: Monaco, Consolas, monospace; }
    pre, code { border: 1px solid #CCC; background: #F0F0F0; }
    pre code, h1 code, h2 code, h3 code, h4 code, h5 code, h6 code { border: none; background: none; }
    pre { padding: .3em; margin: 0 4em 0 2em; }
    code { padding: .05em .2em; }
    pre code { padding: none; }
    p, li { text-align: justify; line-height: 1.75em; }
    p img { display: block; margin-left: 2em; }
    h1 { margin: 0; padding: 0 30% 0 0; }
    h2, h3 { border-bottom: 2px solid #F6F6F6; margin: 2em 0 0 0; padding-left: 0.5em; }
    h1 { color: #AAA; text-shadow: #000 1px 1px 0; }
    h2 a:link, h2 a:visited, h3 a:link, h3 a:visited { padding: .2em; text-decoration: none; color: inherit; }
    h2 a:hover, h3 a:hover { color: #F00; }
    hr { height: 0; border: none; border: 1px solid #F6F6F6; }
    #breadcrumbs { font-weight: bold; }
    #breadcrumbs a:link, #breadcrumbs a:visited { opacity: 0.2; text-decoration: none; padding: .3em; }
    .anchor { float: right; }
    #last_updated { margin-top: 4em; color: #DDD; }
  </style>
 </head>
 <body>]], require 'markdown' (mkd_source), [[
 </body>
</html>]]))
assert(output:close())
