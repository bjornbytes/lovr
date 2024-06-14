group('physics', function()
  local world
  before(function() world = lovr.physics.newWorld() end)

  group('World', function()
    test('distant colliders', function()
      local c1 = world:newBoxCollider(1e8, 0, 0)
      local c2 = world:newBoxCollider(1e8, 0, 0)
      world:update(1)
    end)
  end)
end)
