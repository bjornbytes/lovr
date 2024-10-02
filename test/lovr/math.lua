group('math', function()
  group('Curve', function()
    test(':slice', function()
      local points = {
        vec3(0, 0, 0),
        vec3(0, 1, 0),
        vec3(1, 2, 0),
        vec3(2, 1, 0),
        vec3(2, 0, 0)
      }

      curve = lovr.math.newCurve(points)
      slice = curve:slice(0, 1)
      for i = 1, #points do
        expect({ curve:getPoint(i) }).to.equal({ slice:getPoint(i) })
      end
    end)
  end)

  group('vectors', function()
    test('temporary vector generation errors', function()
      local v = vec3()
      lovr.math.drain()
      expect(function() v:length() end).to.fail.with('Attempt to use a temporary vector from a previous frame')
    end)
  end)
end)
