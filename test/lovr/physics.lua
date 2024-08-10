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

      expect(c:getJoints()).to.equal({})
      expect(c:getShapes()).to.equal({})

      c:setUserData(7)
      expect(c:getUserData()).to.equal(7)

      expect(c:isKinematic()).to.equal(false)
      c:setKinematic(true)
      expect(c:isKinematic()).to.equal(true)
      c:setKinematic(false)

      expect(c:isSensor()).to.equal(false)
      c:setSensor(true)
      expect(c:isSensor()).to.equal(true)

      expect(c:isContinuous()).to.equal(false)
      c:setContinuous(true)
      expect(c:isContinuous()).to.equal(true)

      expect(c:getGravityScale()).to.equal(1.0)
      c:setGravityScale(2.0)
      expect(c:getGravityScale()).to.equal(2.0)

      expect(c:isSleepingAllowed()).to.equal(true)
      c:setSleepingAllowed(false)
      expect(c:isSleepingAllowed()).to.equal(false)

      expect(c:isAwake()).to.equal(false)
      c:setAwake(true)
      expect(c:isAwake()).to.equal(false)

      c:setMass(10)
      expect(c:getMass()).to.equal(10)
      c:resetMassData()

      c:setDegreesOfFreedom('z', 'x')
      expect({ c:getDegreesOfFreedom() }).to.equal({ 'z', 'x' })
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
