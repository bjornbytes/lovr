local lust = require 'lust'

group, test, expect, before = lust.describe, lust.it, lust.expect, lust.before

function lovr.load()
  require('lovr/' .. (arg[1] or 'init'))
  lovr.event.quit(lust.errors > 0 and 1 or 0)
end

function lovr.errhand(message)
  print(message)
end
