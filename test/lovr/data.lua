group('data', function()
  group('Blob', function()
    test(':getName', function()
      -- Test that Blob copies its name instead of relying on Lua string staying live
      blob = lovr.data.newBlob('foo', 'b' .. 'ar')
      collectgarbage()
      expect(blob:getName()).to.equal('b' .. 'ar')
    end)
  end)

  group('Image', function()
    test(':setPixel', function()
      local image = lovr.data.newImage(4, 4)
      image:setPixel(0, 0, 1, 0, 0, 1)
      expect({ image:getPixel(0, 0) }).to.equal({ 1, 0, 0, 1 })

      -- Default alpha
      image:setPixel(1, 1, 0, 1, 0)
      expect({ image:getPixel(1, 1) }).to.equal({ 0, 1, 0, 1 })

      -- Out of bounds
      expect(function() image:setPixel(4, 4, 0, 0, 0, 0) end).to.fail()
      expect(function() image:setPixel(-4, -4, 0, 0, 0, 0) end).to.fail()
    end)
  end)
end)
