--[[

 Documentation generator for the Lua/APR binding.

 Author: Peter Odding <peter@peterodding.com>
 Last Change: October 30, 2011
 Homepage: http://peterodding.com/code/lua/apr/
 License: MIT

]]

-- Files containing documentation fragments (the individual lines enable
-- automatic rebasing between git feature branches and the master branch).
local SOURCES = [[
  base64.c
  crypt.c
  date.c
  dbd.c
  dbm.c
  env.c
  filepath.c
  fnmatch.c
  io_dir.c
  io_file.c
  io_net.c
  io_pipe.c
  ldap.c
  memcache.c
  getopt.c
  http.c
  proc.c
  shm.c
  signal.c
  str.c
  thread.c
  thread_queue.c
  time.c
  uri.c
  user.c
  uuid.c
  xlate.c
  xml.c
  apr.lua
  lua_apr.c
  permissions.c
  errno.c
  ../examples/download.lua
  ../examples/webserver.lua
  ../examples/threaded-webserver.lua
]]

local modules = {}
local sorted_modules = {}

local fmt = string.format
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

local function message(...)
  io.stderr:write(fmt(...), '\n')
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

Read from file, according to the given formats, which specify what to read. For each format, the function returns a string (or a number) with the characters read, or nil if it cannot read data with the specified format. When called without formats, it uses a default format that reads the entire next line (see below).

The available formats are:

 - `'*n'`: reads a number; this is the only format that returns a number instead of a string
 - `'*a'`: reads all data from the file, starting at the current position. On end of input, it returns the empty string
 - `'*l'`: reads the next line (skipping the end of line), returning nil on end of input (this is the default format)
 - `number`: reads a string with up to this number of characters, returning nil on end of input. If number is zero, it reads nothing and returns an empty string, or nil on end of input

[fread]: http://www.lua.org/manual/5.1/manual.html#pdf-file:read ]],
  -- file:write() {{{1
  ['file:write'] = [[
_This function implements the interface of the [file:write()] [fwrite] function described in the Lua 5.1 reference manual. Here is the description from the reference manual:_

Write the value of each argument to file. The arguments must be strings or numbers. To write other values, use `tostring()` or `string.format()` before this function.

[fwrite]: http://www.lua.org/manual/5.1/manual.html#pdf-file:write ]],
  -- file:seek() {{{1
  ['file:seek'] = [[
_This function implements the interface of the [file:seek()] [fseek] function described in the Lua 5.1 reference manual. Here is the description from the reference manual:_

Sets and gets the file position, measured from the beginning of the file, to
the position given by @offset plus a base specified by the string @whence, as
follows:

 - `'set'`:  base is position 0 (beginning of the file)
 - `'cur'`:  base is current position
 - `'end'`:  base is end of file

In case of success, function `seek` returns the final file position, measured
in bytes from the beginning of the file. If this function fails, it returns
nil, plus a string describing the error.

The default value for @whence is `'cur'`, and for offset is 0. Therefore, the
call `file:seek()` returns the current file position, without changing it; the
call `file:seek('set')` sets the position to the beginning of the file (and
returns 0); and the call `file:seek('end')` sets the position to the end of the
file, and returns its size.

[fseek]: http://www.lua.org/manual/5.1/manual.html#pdf-file:seek ]],
  -- file:lines() {{{1
  ['file:lines'] = [[
_This function implements the interface of the [file:lines()] [flines] function described in the Lua 5.1 reference manual. Here is the description from the reference manual:_

Return an iterator function that, each time it is called, returns a new line read from the file. Therefore, the construction

    for line in file:lines() do body end

will iterate over all lines. This function does not close the file when the loop ends.

[flines]: http://www.lua.org/manual/5.1/manual.html#pdf-file:lines ]],
-- }}}1
}

-- Extract test coverage statistics from gcov. {{{1

-- XXX The gcov invocation below used to work fine with multiple source code
-- files (gcov -f *.c) but at some point it broke?! Therefor I've now resorted
-- to running gcov once for each C source code file.

local coverage = {}
for filename in SOURCES:gmatch '%S+' do
  if filename:find '%.c$' and filename ~= 'errno.c' then
    local gcov = assert(io.popen('gcov -o src -nf ' .. filename .. ' 2>/dev/null'))
    local cfun
    local count = 0
    for line in gcov:lines() do
      local test = line:match "^Function '(.-)'$"
      if test then
        cfun = test
      elseif cfun then
        local value = tonumber(line:match "^Lines executed:%s*(%d+%.?%d*)%% of %d+$")
        if value then
          coverage[cfun] = value
          count = count + 1
          cfun = nil
        end
      end
    end
    assert(gcov:close())
  end
end

-- Parse documentation comments in C and Lua source code files. {{{1

local function mungedesc(signature, description)
  local sname = description:match "This function implements the interface of Lua's `([^`]+)%(%)` function."
  if sname and shareddocs[sname] then
    local oldtype = sname:match '^(%w+)'
    local newtype = signature:match '^%s*(%w+)'
    local firstline, otherlines = shareddocs[sname]:match '^(.-)\n\n(.+)$'
    local otherlines, lastline = otherlines:match '^(.+)\n\n(.-)$'
    if newtype == 'shm' then
      otherlines = otherlines:gsub('file:seek', 'shm:seek')
      otherlines = otherlines:gsub('@file', '@shm')
      otherlines = otherlines:gsub('file', 'shared memory')
    end
    otherlines = otherlines:gsub(oldtype, newtype)
    description = firstline .. '\n\n' .. otherlines .. '\n\n' .. lastline
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
      if signature then
        description = mungedesc(signature, description)
        local by = funcbody:find 'luaL_checklstring' or funcbody:find 'datum_check' or funcbody:find 'lua_pushlstring'
        local bn = funcbody:find 'luaL_checkstring' or funcbody:find 'lua_tostring' or funcbody:find 'luaL_optstring' or funcbody:find 'lua_pushstring'
        local binarysafe; if by and not bn then binarysafe = true elseif bn then binarysafe = false end
        table.insert(module.functions, {
          signature = stripfoldmarker(signature),
          description = stripcomment(description),
          binarysafe = binarysafe })
      end
    end
  elseif not filename:find '[\\/]examples[\\/]' then
    local pattern = '\n(\n%-%- (apr%.[%w_]+)%(.-\n%-%-[^\n]*)\n\n'
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

-- Convert documentation comments to Markdown hypertext. {{{1

local function findrelease()
  for line in io.lines 'src/apr.lua' do
    local version = line:match "^apr%._VERSION = '(.-)'"
    if version then return version end
  end
  assert(false, "Failed to determine Lua/APR version number!")
end

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

*This document was generated from the Lua/APR **]] .. findrelease() .. [[** source code.*

[homepage]: http://peterodding.com/code/lua/apr/
[apr_wiki]: http://en.wikipedia.org/wiki/Apache_Portable_Runtime
[lua_wiki]: http://en.wikipedia.org/wiki/Lua_%28programming_language%29
[apr_docs]: http://apr.apache.org/docs/apr/trunk/modules.html

]]) }

function blocks:add(template, ...)
  self[#self+1] = fmt(template, ...)
end

local function toanchor(s)
  s = s:lower()
  s = s:gsub('[^a-z0-9_.:]+', '_')
  s = s:gsub('example:_', 'example_')
  return s
end

local function sig2pubfun(s)
  if s:find '^#result_set' then
    return 'result_set:__len'
  else
    return s:match '^[%w_.:]+'
  end
end

local function sig2privfun(s)
  if s:find '^#' then
    return s:gsub('^#result_set.-$', 'dbr_len')
  else
    s = s:match '^[%w_.:]+'
    s = s:gsub('%W', '_')
    s = s:gsub('^directory_', 'dir_')
    s = s:gsub('^process_', 'proc_')
    s = s:gsub('^driver_', 'dbd_')
    s = s:gsub('^prepared_statement_', 'dbp_')
    s = s:gsub('^result_set_', 'dbr_')
    s = s:gsub('^xml_parser_', 'xml_')
    s = s:gsub('^mc_client_', 'mc_')
    s = s:gsub('^ldap_conn', 'lua_apr_ldap')
    return s
  end
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
            return fmt('[%s()](%s)', funcname:gsub('_', '\\_'), target)
          end))
  end
  return table.concat(output, '\n\n')
end

blocks:add '## Table of contents'
local lines = { '<dl>' }
local misc_entries = {}
for _, module in ipairs(sorted_modules) do
  if not module.functions[1] then
    table.insert(misc_entries, module)
  else
    lines[#lines + 1] = fmt('<dt style="margin: 1em 0; font-weight: bold"><a href="#%s">%s</a></dt>', toanchor(module.name), module.name)
    lines[#lines + 1] = '<dd><ul>'
    for _, entry in ipairs(module.functions) do
      local signature = entry.signature:match '^#?[%w.:_]+'
      local anchor = toanchor(sig2pubfun(signature))
      if not signature:find '#' then signature = signature .. '()' end
      lines[#lines + 1] = fmt('<li style="float: left; width: 16em"><a href="#%s" style="text-decoration:none">%s</a></li>', anchor, signature)
    end
    lines[#lines + 1] = '</ul><br style="clear: both"></dd>'
    -- lines[#lines + 1] = ''
    -- blocks:add('%s', table.concat(lines, '\n'))
  end
end
lines[#lines + 1] = '<dt style="padding: 1em 0; font-weight: bold">Miscellaneous sections</dt>'
lines[#lines + 1] = '<dd><ul>'
for _, module in ipairs(misc_entries) do
  lines[#lines + 1] = fmt('<li><a href="#%s">%s</a></li>', toanchor(module.name), module.name)
end
lines[#lines + 1] = '</ul></dd>'
lines[#lines + 1] = '</dl>'
blocks:add('%s', table.concat(lines, '\n'))

local bsignore = {
  ['apr.dbd'] = true,
  ['apr.ldap'] = true,
  ['apr.ldap_info'] = true,
  ['apr.ldap_url_check'] = true,
  ['apr.ldap_url_parse'] = true,
  ['apr.os_default_encoding'] = true,
  ['apr.os_locale_encoding'] = true,
  ['apr.parse_cookie_header'] = true,
  ['apr.parse_headers'] = true,
  ['apr.parse_multipart'] = true,
  ['apr.parse_query_string'] = true,
  ['apr.platform_get'] = true,
  ['apr.proc_fork'] = true,
  ['apr.signal_names'] = true,
  ['apr.strfsize'] = true,
  ['apr.type'] = true,
  ['apr.uri_port_of_scheme'] = true,
  ['apr.uuid_format'] = true,
  ['apr.uuid_get'] = true,
  ['apr.uuid_parse'] = true,
  ['apr.version_get'] = true,
  ['driver:driver'] = true,
  ['driver:transaction_mode'] = true,
  ['file:lock'] = true,
  ['ldap_conn:bind'] = true,
  ['ldap_conn:option_get'] = true,
  ['ldap_conn:option_set'] = true,
  ['ldap_conn:rebind_add'] = true,
  ['ldap_conn:search'] = true,
  ['mc_client:add'] = true,
  ['mc_client:add_server'] = true,
  ['mc_client:add_server'] = true,
  ['mc_client:decr'] = true,
  ['mc_client:delete'] = true,
  ['mc_client:find_server'] = true,
  ['mc_client:find_server'] = true,
  ['mc_client:get'] = true,
  ['mc_client:incr'] = true,
  ['mc_client:replace'] = true,
  ['mc_client:set'] = true,
  ['mc_client:stats'] = true,
  ['mc_client:version'] = true,
  ['md5_context:digest'] = true,
  ['sha1_context:digest'] = true,
  ['socket:listen'] = true,
  ['thread:join'] = true,
  ['thread:status'] = true,
  ['xml_parser:geterror'] = true,
}
local function dumpentries(functions)
  for _, entry in ipairs(functions) do
    local signature = entry.signature:gsub('%->', '→')
    local funcname = sig2pubfun(signature)
    local anchor = toanchor(funcname)
    local tc = ''
    if next(coverage) then
      local covkey = sig2privfun(signature)
      if not coverage[covkey] then covkey = 'lua_' .. covkey end
      tc = coverage[covkey] or ''
      if tc ~= '' then
        local template = '<span style="float: right; font-size: small; color: %s; opacity: 0.5">test coverage: %s<br></span>'
        local color = tc >= 75 and 'green' or tc >= 50 and 'orange' or 'red'
        tc = fmt(template, color, tc == 0 and 'none' or fmt('%i%%', tc))
      end
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

local custom_sorting = {
  ['crypt.c'] = [[ apr.md5 apr.md5_encode apr.password_validate
    apr.password_get apr.md5_init md5_context:update md5_context:digest
    md5_context:reset apr.sha1 apr.sha1_init sha1_context:update
    sha1_context:digest sha1_context:reset ]],
  ['thread.c'] = [[ apr.thread apr.thread_create apr.thread_yield
    thread:status thread:join ]],
}

for _, module in ipairs(sorted_modules) do

  local a = toanchor(module.name)
  blocks:add('## <a name="%s" href="#%s">%s</a>', a, a, module.name)
  if module.header then blocks:add('%s', preprocess(module.header)) end
  if module.example then
    blocks:add('    %s', module.example:gsub('\n', '\n    '))
  end

  if custom_sorting[module.file] then
    local ordertbl = { n = 0 }
    for f in custom_sorting[module.file]:gmatch '%S+' do
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
io.write(table.concat(blocks, '\n\n'))

