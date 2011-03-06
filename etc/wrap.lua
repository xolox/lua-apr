-- Load Markdown module (lua-discount or markdown.lua)
status, markdown = pcall(require, 'discount')
if not status then markdown = require 'markdown' end
assert(markdown, "Failed to load Markdown filter")

-- Parse and convert the markdown to HTML.
input = io.read('*all')
title = input:match '^%s*#%s*([^\r\n]+)'
content = markdown(input)

-- Workaround for bug in Discount :-(
content = content:gsub('<code>&ndash;</code>', '<code>-</code>')

-- Syntax highlight snippets of Lua source code in the documentation.
status, highlighter = pcall(require, 'lxsh.highlighters.lua')
if status and highlighter then
  local html_entities = { amp = '&', lt = '<', gt = '>' }
  content = content:gsub('<pre>(.-)</pre>', function(block)
    block = block:match '^<code>(.-)</code>$' or block
    block = block:gsub('&(%w+);', html_entities)
    return highlighter(block, { encodews = true })
  end)
end

-- Wrap the HTML in a standardized template.
io.write([[
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN"
"http://www.w3.org/TR/html4/strict.dtd">
<html>
<head>
<title>]], title, [[</title>
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
<body>
]], content, [[
</body>
</html>
]])
