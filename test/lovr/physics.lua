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

  group('Collider', function()
    test(':setEnabled', function()
      local c = world:newCollider()
      expect(c:isEnabled()).to.equal(true)
      c:setEnabled()
      expect(c:isEnabled()).to.equal(false)
      c:setEnabled()
      expect(c:isEnabled()).to.equal(false)
      c:setEnabled(true)
      expect(c:isEnabled()).to.equal(true)
    end)
  end)

  group('MeshShape', function()
    if lovr.graphics then
      test('from Mesh', function()
        mesh = lovr.graphics.newMesh({
          {   0,  .4, 0 },
          { -.5, -.4, 0 },
          {  .5, -.4, 0 }
        })

        shape = lovr.physics.newMeshShape(mesh)
      end)
    end
  end)
end)
