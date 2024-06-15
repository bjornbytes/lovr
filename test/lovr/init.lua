group('lovr', function()
  test('getVersion', function()
    expect({ lovr.getVersion() }).to.equal({ 0, 17, 1, 'Tritium Gourmet' })
  end)

  for i, file in ipairs(lovr.filesystem.getDirectoryItems('lovr')) do
    local module = file:match('%a+')
    if lovr[module] then
      require('lovr/' .. module)
    end
  end
end)
