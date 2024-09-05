group('physics', function()
  local world
  before(function() world = lovr.physics.newWorld() end)

  group('World', function()
    test('distant colliders', function()
      local c1 = world:newBoxCollider(1e8, 0, 0)
      local c2 = world:newBoxCollider(1e8, 0, 0)
      world:update(1)
    end)

    group(':raycast', function()
      test('zero-shape Collider', function()
        collider = world:newCollider(0, 0, 0)
        world:raycast(0, 10, 0, 0, -10, 0)
      end)
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

  group('Shape', function()
    test(':raycast', function()
      shape = lovr.physics.newBoxShape(2, 10, 2)
      expect(shape:raycast(-10, 10, 0,  10, 10, 0)).to_not.be.truthy()
      expect({ shape:raycast(-10, 0, 0,  10, 0, 0) }).to.equal({ -1, 0, 0, -1, 0, 0 }, 1e-6)
      expect({ shape:raycast(-10, 4, 0,  10, 4, 0) }).to.equal({ -1, 4, 0, -1, 0, 0 }, 1e-6)
      shape:setOffset(0, 0, 0, math.pi / 2, 0, 0, 1)
      expect({ shape:raycast(-10, 0, 0,  10, 0, 0) }).to.equal({ -5, 0, 0, -1, 0, 0 }, 1e-6)
      expect(shape:raycast(-10, 4, 0,  10, 4, 0)).to.equal(nil)

      collider = world:newCollider(100, 100, 100)
      collider:addShape(shape)
      expect(shape:raycast(-10, 0, 0, 10, 0, 0)).to.equal(nil)
      expect({ shape:raycast(-500, 100, 100, 500, 100, 100) }).to.equal({ 95, 100, 100, -1, 0, 0 }, 1e-6)
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
end)
