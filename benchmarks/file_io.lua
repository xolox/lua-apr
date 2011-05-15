#!/usr/bin/env lua

--[[

 Benchmarks of the different formats supported by file:read() as implemented in
 the Lua standard library vs. the Lua/APR binding. Requires gnuplot to generate
 graphs in PNG format. See http://peterodding.com/code/lua/apr/benchmarks for
 examples of the generated graphs.

--]]

local apr = require 'apr'

local function msg(...)
  io.stderr:write(string.format(...), '\n')
end

local formats = {{
  format = '*a',
  graph = 'file-read-all.png',
  title = "Performance of file:read('*a')",
  generate = function(size)
    local random = io.open('/dev/urandom')
    local data = random:read(math.min(size, 1024 * 512))
    random:close()
    if #data < size then
      data = data:rep(size / #data)
    end
    return data
  end,
}, {
  format = '*l',
  graph = 'file-read-lines.png',
  title = "Performance of file:read('*l')",
  generate = function(size)
    -- Generate a line with random printable characters.
    local line = {}
    local char = string.char
    local random = math.random
    for i = 1, 512 do line[#line + 1] = char(random(0x20, 0x7E)) end
    line = table.concat(line)
    -- Generate a few lines of random lengths.
    local lines = {}
    local count = 0
    while #lines < 100 do
      local cline = line:sub(1, math.random(1, #line))
      lines[#lines + 1] = cline
      size = size + #cline
    end
    lines = table.concat(lines, '\n')
    -- Now generate the requested size in lines.
    return lines:rep(size / #lines)
  end,
}, {
  format = '*n',
  graph = 'file-read-numbers.png',
  title = "Performance of file:read('*n')",
  generate = function(size)
    local numbers = {}
    for i = 1, 100 do
      numbers[#numbers + 1] = tostring(math.random())
    end
    numbers = table.concat(numbers, '\n')
    return numbers:rep(size / #numbers)
  end,
}}

local function benchformat(func, path, format, size, label)
  local best
  for i = 1, 2 do
    local start = apr.time_now()
    local handle = func(path)
    repeat
      local result = handle:read(format)
    until format == '*a' and result == '' or not result
    handle:close()
    local total = apr.time_now() - start
    msg('%17s: read %s with file:read(%s) at %s/s',
        label, apr.strfsize(size), format, apr.strfsize(size / total))
    best = best and best < total and best or total
  end
  return { time = best, size = size / 1024 / 1024 }
end

local function writeresults(fname, data)
  local handle = io.open(fname, 'w')
  for _, sample in ipairs(data) do
    handle:write(sample.size, ' ', sample.time, '\n')
  end
  handle:close()
end

local datafile = os.tmpname() .. '.bin'
local luafile = os.tmpname() .. '.dat'
local aprfile = os.tmpname() .. '.dat'

for _, format in ipairs(formats) do
  local luatimes = {{ time = 0, size= 0 }}
  local aprtimes = {{ time = 0, size= 0 }}
  local size = 1024 * 1024
  local max = 1024 * 1024 * 25
  local data = format.generate(max)
  while size <= max do
    local handle = io.open(datafile, 'w')
    handle:write(data:sub(1, size))
    handle:close()
    table.insert(luatimes, benchformat(io.open, datafile, format.format, size, "Standard library"))
    table.insert(aprtimes, benchformat(apr.file_open, datafile, format.format, size, "Lua/APR"))
    size = size + 1024 * 1024
  end
  writeresults(luafile, luatimes)
  writeresults(aprfile, aprtimes)
  local gnuplot = io.popen('gnuplot', 'w')
  gnuplot:write(string.format([[
    set title "%s"
    set xlabel "size in megabytes"
    set ylabel "time in seconds"
    set term png
    set output '%s'
    plot '%s' with lines lt 3 title "Lua standard library", \
         '%s' with lines lt 4 title "Lua/APR binding"
  ]], format.title, format.graph, luafile, aprfile))
  gnuplot:close()
end

os.remove(datafile)
os.remove(luafile)
os.remove(aprfile)
