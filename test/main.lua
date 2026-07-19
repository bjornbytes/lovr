local lust = require 'lust'

group, test, expect, before = lust.describe, lust.it, lust.expect, lust.before

function lovr.load()
  for i, argv in ipairs(arg) do
    if argv == '-f' or argv == '--focus' then
      lust.item_filter = function(name)
        return tostring(name):match('#focus$')
      end
    end
  end

  local module = arg[1] and arg[1]:match('^%w+') or 'init'
  require('lovr/' .. module)
  print(string.format("%d passes, %d errors", lust.passes, lust.errors))
  lovr.event.quit(lust.errors > 0 and 1 or 0)
end

function lovr.errhand(message)
  print(message)
end
