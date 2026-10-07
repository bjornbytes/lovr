group('physics', function()
  local world
  before(function() world = lovr.physics.newWorld() end)

  group('World', function()
    test(':getTags', function()
      expect(world:getTags()).to.equal({})

      local taggedWorld = lovr.physics.newWorld({ tags = { 'a', 'b' } })
      expect(taggedWorld:getTags()).to.equal({ 'a', 'b' })
    end)

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
      expect(c:getDegreesOfFreedom()).to.equal('z', 'x')
    end)

    test(':setDegreesOfFreedom', function()
      local ball = world:newSphereCollider(0, 0, 0, 10)
      ball:setMass(.6)
      ball:setDegreesOfFreedom('xyz', 'xyz')
      expect(ball:getMass()).to.approximately.equal(.6)
    end)
  end)

  group('Shape', function()
    test(':raycast', function()
      shape = lovr.physics.newBoxShape(2, 10, 2)
      expect(shape:raycast(-10, 10, 0,  10, 10, 0)).to_not.be.truthy()
      expect(shape:raycast(-10, 0, 0,  10, 0, 0)).to.approximately.equal(-1, 0, 0, -1, 0, 0, nil)
      expect(shape:raycast(-10, 4, 0,  10, 4, 0)).to.approximately.equal(-1, 4, 0, -1, 0, 0, nil)
      shape:setOffset(0, 0, 0, math.pi / 2, 0, 0, 1)
      expect(shape:raycast(-10, 0, 0,  10, 0, 0)).to.approximately.equal(-5, 0, 0, -1, 0, 0, nil)
      expect(shape:raycast(-10, 4, 0,  10, 4, 0)).to.equal()

      collider = world:newCollider()
      collider:setPosition(100, 100, 100)
      collider:setOrientation(math.pi, 0, 1, 0)
      collider:addShape(shape)
      expect(shape:raycast(-10, 0, 0, 10, 0, 0)).to.equal()
      expect(shape:raycast(-500, 100, 100, 500, 100, 100)).to.approximately.equal(95, 100, 100, -1, 0, 0, nil)
    end)

    group('CapsuleShape', function()
      test(':setRadius', function()
        shape = lovr.physics.newCapsuleShape(1, 2)
        expect(function() shape:setRadius(0) end).to.fail()
        expect(function() shape:setRadius(-3) end).to.fail()
      end)

      test(':setLength', function()
        shape = lovr.physics.newCapsuleShape(1, 2)
        expect(function() shape:setLength(0) end).to.fail()
        expect(function() shape:setLength(-3) end).to.fail()
      end)
    end)

    group('CylinderShape', function()
      test(':setRadius', function()
        shape = lovr.physics.newCylinderShape(1, 2)
        expect(function() shape:setRadius(0) end).to.fail()
        expect(function() shape:setRadius(-3) end).to.fail()
      end)

      test(':setLength', function()
        shape = lovr.physics.newCylinderShape(1, 2)
        expect(function() shape:setLength(0) end).to.fail()
        expect(function() shape:setLength(-3) end).to.fail()
      end)
    end)

    group('ConvexShape', function()
      test('invalid', function()
        local invalid = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
        expect(function() lovr.physics.newConvexShape(invalid) end).to.fail()
      end)

      test('tetrahedron', function()
        shape = lovr.physics.newConvexShape({ 1, 1, 0, -1, 1, 0, 0, 0, 0, 0, 0, 1 })
        expect(shape:getPointCount()).to.equal(4)
        expect(shape:getFaceCount()).to.equal(4)
        expect(shape:getFace(1)).to.equal({ 1, 2, 3 })
        expect(shape:getFace(2)).to.equal({ 2, 4, 3 })
        expect(shape:getFace(3)).to.equal({ 4, 1, 3 })
        expect(shape:getFace(4)).to.equal({ 1, 4, 2 })
      end)

      if lovr.graphics then
        test('from Mesh', function()
          mesh = lovr.graphics.newMesh({
            { 1, 1, 0 },
            { -1, 1, 0 },
            { 0, 0, 0 },
            { 0, 0, 1 }
          })
          mesh:setIndices({ 1, 2, 3, 1, 3, 4, 1, 2, 4, 2, 3, 4 })
          shape = lovr.physics.newConvexShape(mesh)
          expect(shape:getPointCount()).to.equal(4)
        end)
      end

      test('scale', function()
        shape = lovr.physics.newConvexShape({ 1, 1, 0, -1, 1, 0, 0, 0, 0, 0, 0, 1 }, 2)
        expect(shape:getScale()).to.equal(2, 2, 2)
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

      test('scale', function()
        shape = lovr.physics.newMeshShape({
          {   0,  .4, 0 },
          { -.5, -.4, 0 },
          {  .5, -.4, 0 }
        }, { 1, 2, 3 }, 5, 1, 1)

        expect(shape:getScale()).to.equal(5, 1, 1)
      end)
    end)
  end)
end)
