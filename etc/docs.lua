--[[

 Documentation generator for the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: January 2, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

]]

local SOURCES = [[ base64.c crypt.c date.c dbm.c env.c filepath.c fnmatch.c
  io_dir.c io_file.c io_net.c io_pipe.c proc.c str.c time.c uri.c user.c
  uuid.c xlate.c apr.lua lua_apr.c permissions.c errno.c
  ../examples/download.lua ../examples/webserver.lua ]]

local modules = {}
local sorted_modules = {}

local function trim(s)
  return s:match '^%s*(.-)%s*$'
end

local function getmodule(name, file, header)
  local key = name:lower()
  local value = modules[key]
  if not value then
    local metadata, newheader = (header  .. '\n\n'):match '\n\n(.-)\n\n(.-)$' -- strip metadata
    header = (newheader or ''):find '%S' and trim(newheader) or nil
    value = {name=name, file=file, header=header, functions={}}
    modules[key] = value
    table.insert(sorted_modules, value)
  end
  return value
end

local function message(string, ...)
  io.stderr:write(string:format(...), '\n')
end

local function stripfoldmarker(s)
  return trim(s:gsub('{{%{%d', ''))
end

local function stripcomment(s)
  return trim(s:gsub('\n %* ?', '\n'))
end

local shareddocs = {
  -- file:read() {{{1
  ['file:read'] = [[
_This function implements the interface of the [file:read()] [fread] function described in the Lua 5.1 reference manual. Here is the description from the reference manual:_

Read from @file, according to the given formats, which specify what to read. For each format, the function returns a string (or a number) with the characters read, or nil if it cannot read data with the specified format. When called without formats, it uses a default format that reads the entire next line (see below).

The available formats are:

 - `'*n'`: reads a number; this is the only format that returns a number instead of a string
 - `'*a'`: reads all data from the @file, starting at the current position. On end of input, it returns the empty string
 - `'*l'`: reads the next line (skipping the end of line), returning nil on end of input (this is the default format)
 - `number`: reads a string with up to this number of characters, returning nil on end of input. If number is zero, it reads nothing and returns an empty string, or nil on end of input
   
[fread]: http://www.lua.org/manual/5.1/manual.html#pdf-file:read ]],
  -- file:write() {{{1
  ['file:write'] = [[
_This function implements the interface of the [file:write()] [fwrite] function described in the Lua 5.1 reference manual. Here is the description from the reference manual:_

Write the value of each argument to @file. The arguments must be strings or numbers. To write other values, use `tostring()` or `string.format()` before this function.

[fwrite]: http://www.lua.org/manual/5.1/manual.html#pdf-file:write ]],
  -- file:lines() {{{1
  ['file:lines'] = [[
_This function implements the interface of the [file:lines()] [flines] function described in the Lua 5.1 reference manual. Here is the description from the reference manual:_

Return an iterator function that, each time it is called, returns a new line read from the @file. Therefore, the construction

    for line in file:lines() do body end

will iterate over all lines. This function does not close the @file when the loop ends.

[flines]: http://www.lua.org/manual/5.1/manual.html#pdf-file:lines ]],
-- }}}1
}

-- Parse documentation comments in C and Lua source code files. {{{1

local function mungedesc(signature, description)
  local sname = description:match "This function implements the interface of Lua's `([^`]+)%(%)` function."
  if sname and shareddocs[sname] then
    local oldtype = sname:match '^(%w+)'
    local newtype = signature:match '^%s*(%w+)'
    local firstline, otherlines = shareddocs[sname]:match '^(.-)\n\n(.+)$'
    description = firstline .. '\n\n' .. otherlines:gsub(oldtype, newtype)
  end
  return description
end

for filename in SOURCES:gmatch '%S+' do
  local handle = assert(io.open('src/' .. filename))
  local source = assert(handle:read('*all')):gsub('\r\n', '\n')
  handle:close()
  if filename:find '%.c$' then
    -- Don't extract documentation from ignored C source code.
    source = source:gsub('\n#if 0.-\n#endif', '')
    local header = stripcomment(source:match '^/%*(.-)%*/' or '')
    local modulename = header:match '^([^\n]-) module'
    if not modulename then
      message("%s: Failed to determine module name!", filename)
    end
    local module = getmodule(modulename, filename, header)
    local pattern = '\n/%*([^\n]-%->.-)%*/\n\n%w[^\n]- ([%w_]+)%([^\n]-%)\n(%b{})'
    for docblock, funcname, funcbody in source:gmatch(pattern) do
      local signature, description = docblock:match '^([^\n]- %-> [^\n]+)(\n.-)$'
      if not signature then
        message("%s: Function %s doesn't have a signature?!", filename, funcname)
      else
        description = mungedesc(signature, description)
        local by = funcbody:find 'luaL_checklstring' or funcbody:find 'datum_check' or funcbody:find 'lua_pushlstring'
        local bn = funcbody:find 'luaL_checkstring' or funcbody:find 'lua_tostring'
        local binarysafe; if by and not bn then binarysafe = true elseif bn then binarysafe = false end
        table.insert(module.functions, {
          signature = stripfoldmarker(signature),
          description = stripcomment(description),
          binarysafe = binarysafe })
      end
    end
  elseif not filename:find '[\\/]examples[\\/]' then
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
  else
    local header, modulename, example = source:match '^%-%-%[%[%s+(([^\n]+).-)%]%]\n(.-)$'
    modulename = trim(modulename)
    header = trim(header:gsub('\n  ', '\n'))
    getmodule(modulename, filename, header).example = trim(example)
  end
end

-- Extract test coverage statistics from gcov. {{{1

local gcov = assert(io.popen 'cd src && gcov -f *.c 2>/dev/null')
local cfun
local coverage = {}
local count = 0
for line in gcov:lines() do
  local test = line:match "^Function '(.-)'$"
  if test then
    cfun = test
  elseif cfun then
    coverage[cfun] = tonumber(line:match "^Lines executed:%s*(%d+%.?%d*)%% of %d+$")
    count = count + 1
    cfun = nil
  end
end

-- Convert documentation comments to Markdown hypertext. {{{1

local blocks = { trim([[

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

*This document was generated from the Lua/APR **]] .. apr._VERSION .. [[** source code.*

[homepage]: http://peterodding.com/code/lua/apr/
[apr_wiki]: http://en.wikipedia.org/wiki/Apache_Portable_Runtime
[lua_wiki]: http://en.wikipedia.org/wiki/Lua_%28programming_language%29
[apr_docs]: http://apr.apache.org/docs/apr/trunk/modules.html

]]) }

function blocks:add(string, ...)
  self[#self+1] = string:format(...)
end

local function toanchor(s)
  return s:lower():gsub('[^a-z0-9_.:]+', '_')
end

local function gsplit(string, pattern, capture_delimiters)
  return coroutine.wrap(function()
    local index = 1
    repeat
      local first, last = string:find(pattern, index)
      if first and last then
        if index < first then coroutine.yield(string:sub(index, first - 1)) end
        if capture_delimiters then coroutine.yield(string:sub(first, last)) end
        index = last + 1
      else
        if index <= #string then coroutine.yield(string:sub(index)) end
        break
      end
    until index > #string
  end)
end

local function preprocess(text)
  local output = {}
  for block in gsplit(text, '\n\n', false) do
    output[#output + 1] = block:find '^    ' and block or (block
          :gsub('@permissions', '[permissions](#file_system_permissions)')
          :gsub('error handling', '[error handling](#error_handling)')
          :gsub('(%s)@([%w_]+)', '%1<tt>*%2*</tt>')
          :gsub('([%s])true([%s%p])', '%1<tt>*true*</tt>%2')
          :gsub('([%s%p])false([%s%p])', '%1<tt>*false*</tt>%2')
          :gsub('([%s%p])nil([%s%p])', '%1<tt>*nil*</tt>%2')
          :gsub('`([%w_.:]+)%(%)`', function(funcname)
            local target
            if funcname:find '^apr%.[%w_]+$' or funcname:find '^[%w_]+:[%w_]+$' then
              target = '#' .. toanchor(funcname)
            else
              target = 'http://www.lua.org/manual/5.1/manual.html#pdf-' .. funcname
            end
            return ('[%s()](%s)'):format(funcname:gsub('_', '\\_'), target)
          end))
  end
  return table.concat(output, '\n\n')
end

blocks:add '## Table of contents'
local items = {}
for _, module in ipairs(sorted_modules) do
  items[#items + 1] = (' - [%s](#%s)'):format(module.name, toanchor(module.name))
end
blocks:add('%s', table.concat(items, '\n'))

local bsignore = {
  ['apr.strfsize'] = true,
  ['apr.uri_port_of_scheme'] = true,
  ['apr.uuid_format'] = true,
  ['apr.uuid_get'] = true,
  ['apr.uuid_parse'] = true,
  ['file:lock'] = true,
  ['md5_context:digest'] = true,
  ['sha1_context:digest'] = true,
}
local function dumpentries(functions)
  for _, entry in ipairs(functions) do
    local signature = entry.signature:gsub('%->', '→')
    local funcname = assert(signature:match '^[%w_.:]+')
    local anchor = toanchor(funcname)
    local covkey = funcname:gsub('%W', '_')
                           :gsub('^directory_', 'dir_')
                           :gsub('^process_', 'proc_')
    if not coverage[covkey] then covkey = 'lua_' .. covkey end
    local tc = coverage[covkey] or ''
    if tc ~= '' then
      local template = '<span style="float: right; font-size: small; color: %s; opacity: 0.5">test coverage: %s<br></span>'
      local color = tc >= 75 and '#006600' or tc >= 50 and '#FFCC00' or '#CC0000'
      tc = string.format(template, color, tc == 0 and 'none' or string.format('%i%%', tc))
    end
    blocks:add('### %s <a name="%s" href="#%s">`%s`</a>', tc, anchor, anchor, signature)
    blocks:add('%s', preprocess(entry.description))
    if entry.binarysafe ~= nil and not bsignore[funcname] then
      blocks:add('*This function %s binary safe.*', entry.binarysafe and 'is' or 'is not')
    end
  end
end

local function htmlencode(s)
  return (s:gsub('&', '&amp;')
           :gsub('<', '&lt;')
           :gsub('>', '&gt;'))
end

for _, module in ipairs(sorted_modules) do
  local a = toanchor(module.name)
  blocks:add('## <a name="%s" href="#%s">%s</a>', a, a, module.name)
  if module.header then blocks:add('%s', preprocess(module.header)) end
  if module.example then
    blocks:add('    %s', module.example:gsub('\n', '\n    '))
  end
  if module.file == 'crypt.c' then
    -- Custom sorting for the cryptography module.
    local orderstr = [[ apr.md5 apr.md5_encode apr.password_validate
        apr.password_get apr.md5_init md5_context:update md5_context:digest
        md5_context:reset apr.sha1 apr.sha1_init sha1_context:update
        sha1_context:digest sha1_context:reset ]]
    local ordertbl = { n = 0 }
    for f in orderstr:gmatch '%S+' do
      ordertbl[f], ordertbl.n = ordertbl.n, ordertbl.n + 1
    end
    local function order(f)
      return ordertbl[f.signature:match '^[%w_.:]+']
    end
    table.sort(module.functions, function(a, b)
      return order(a) < order(b)
    end)
  end
  dumpentries(module.functions)
end

-- Join the blocks of Markdown source and write them to the 1st file.
local mkd_source = table.concat(blocks, '\n\n')
local output = assert(io.open(arg[1], 'w'))
assert(output:write(mkd_source))
assert(output:close())

-- Convert Markdown to HTML and apply page template. {{{1

local status, markdown = pcall(require, 'discount')
if not status then markdown = require 'markdown' end

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
    a:link, a:visited { color: #000080; }
    a:hover, a:active { color: #F00; }
    pre, code, tt { font-family: Monaco, Consolas, monospace; }
    pre, code { border: 1px solid #CCC; background: #F0F0F0; }
    pre code, h1 code, h2 code, h3 code, h4 code, h5 code, h6 code { border: none; background: none; }
    pre { padding: .3em; margin: 0 4em 0 2em; }
    code { padding: .05em .2em; }
    pre code { padding: none; }
    p, li { text-align: justify; line-height: 1.75em; }
    h1 { margin: 0; padding: 0 30% 0 0; color: #AAA; text-shadow: #000 1px 1px 0; }
    h2, h3 { border-bottom: 2px solid #F6F6F6; margin: 2em 0 0 0; padding-left: 0.5em; }
    h2 a:link, h2 a:visited, h3 a:link, h3 a:visited { padding: .2em; text-decoration: none; color: inherit; }
    h2 a:hover, h3 a:hover { color: #F00; }
  </style>
 </head>
 <body>]], markdown(mkd_source), [[
 </body>
</html>]]))
assert(output:close())
