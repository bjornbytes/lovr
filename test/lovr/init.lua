group('lovr', function()
  for i, file in ipairs(lovr.filesystem.getDirectoryItems('lovr')) do
    local module = file:match('%a+')
    if lovr[module] then
      require('lovr/' .. module)
    end
  end
end)
