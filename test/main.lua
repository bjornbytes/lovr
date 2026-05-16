local lust = require 'lust'

group, test, expect, before = lust.describe, lust.it, lust.expect, lust.before

function lovr.load()
  local module = arg[1] and arg[1]:match('^%w+') or 'init'
  require('lovr/' .. module)
  print(string.format("%i passes, %i errors", lust.passes, lust.errors))
  lovr.event.quit(lust.errors > 0 and 1 or 0)
end

function lovr.errhand(message)
  print(message)
end
