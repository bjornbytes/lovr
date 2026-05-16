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

  group('Mat4', function()
    test(':set', function()
      local position = vector(1, 2, 3)
      local rotation = quaternion(1.2, 1, 0, 0)
      local scale = vector(1.5, 2.5, 3.5)
      local matrix = lovr.math.newMat4()

      matrix:set(position, scale, rotation)
      expect({ matrix:unpack() }).to.equal({ 1, 2, 3, 1.5, 2.5, 3.5, 1.2, 1, 0, 0 }, 1e-4)

      matrix:set(position, scale)
      expect({ matrix:unpack() }).to.equal({ 1, 2, 3, 1.5, 2.5, 3.5, 0, 0, 0, 0 }, 1e-4)

      matrix:set(position, { x = 1.5, y = 2.5, z = 3.5 })
      expect({ matrix:unpack() }).to.equal({ 1, 2, 3, 1.5, 2.5, 3.5, 0, 0, 0, 0 }, 1e-4)

      matrix:set(position, rotation)
      expect({ matrix:unpack() }).to.equal({ 1, 2, 3, 1, 1, 1, 1.2, 1, 0, 0 }, 1e-4)

      matrix:set(position, 1.2, 1, 0, 0)
      expect({ matrix:unpack() }).to.equal({ 1, 2, 3, 1, 1, 1, 1.2, 1, 0, 0 }, 1e-4)

      matrix:set(position, { rotation:unpack() })
      expect({ matrix:unpack() }).to.equal({ 1, 2, 3, 1, 1, 1, 1.2, 1, 0, 0 }, 1e-4)
    end)

    group(':mul', function()
      test('Mat4', function()
        local a = lovr.math.newMat4():perspective(math.rad(80), 1440 / 900, .01, 0)
        local b = lovr.math.newMat4(vector(0, 1.7, 0), quaternion.pack(0, 0, 0, 1)):invert()
        local r = {
          0.74484598636627, 0, 0, 0,
          0, -1.1917536258698, 0, 0,
          0, 0, 0, -1,
          0, 2.0259811878204, 0.0099999997764826, 0
        }
        expect({ (a * b):unpack(true) }).to.equal(r, 1e-4)
        expect({ (a * b):unpack(true) }).to.equal(r, 1e-4)
        expect({ (a:mul(b)):unpack(true) }).to.equal(r, 1e-4)
      end)

      test('vector', function()
        v = vector(1, 2, 3)
        array = setmetatable({ 1, 2, 3 }, {})
        keyval = setmetatable({ x = 1, y = 2, z = 3 }, {})

        matrix = lovr.math.newMat4(vector(0, 2, 0), quaternion(0, 0, 0, 1))

        expect(matrix * array).to.equal({ 1, 4, 3 })
        expect(matrix:mul(array)).to.equal({ 1, 4, 3 })
        expect(matrix * v).to.equal(vector(1, 4, 3))
        expect(matrix:mul(v)).to.equal(vector(1, 4, 3))
        expect(matrix * keyval).to.equal({ x = 1, y = 4, z = 3 })
        expect(matrix:mul(keyval)).to.equal({ x = 1, y = 4, z = 3 })

        expect(getmetatable(matrix * array)).to.be(getmetatable(array))
        expect(getmetatable(matrix * keyval)).to.be(getmetatable(keyval))
        expect(getmetatable(matrix * v)).to.be(getmetatable(v))
      end)
    end)

    test(':setPosition', function()
      matrix = lovr.math.newMat4():setPosition(1, 2, 3)
      expect({ matrix:unpack() }).to.equal({ 1,2,3, 1,1,1, 0,0,0,0 }, 1e-4)
      matrix:setPosition(vector(3, 4, 5))
      expect({ matrix:unpack() }).to.equal({ 3,4,5, 1,1,1, 0,0,0,0 }, 1e-4)
    end)

    test(':getOrientation', function()
      matrix = lovr.math.newMat4():target({0, 1, -1}, {0, 1.7, 0})
      expect({ matrix:getOrientation() }).to.equal({ 3.1415, 0, 0.9537, -0.3006 }, 1e-4)
    end)

    test(':setOrientation', function()
      matrix = lovr.math.newMat4():setOrientation(1, 0, 1, 0)
      expect({ matrix:unpack() }).to.equal({ 0,0,0, 1,1,1, 1,0,1,0 }, 1e-4)
      matrix:setScale(2)
      matrix:setOrientation(quaternion(2, 1, 0, 0))
      expect({ matrix:unpack() }).to.equal({ 0,0,0, 2,2,2, 2,1,0,0 }, 1e-4)
    end)

    test(':setScale', function()
      matrix = lovr.math.newMat4():setScale(1.5, 2.5, 3.5)
      expect({ matrix:unpack() }).to.equal({ 0,0,0, 1.5,2.5,3.5, 0,0,0,0 }, 1e-4)
      matrix:setScale(vector(3.5, 4.5, 5.5))
      expect({ matrix:unpack() }).to.equal({ 0,0,0, 3.5,4.5,5.5, 0,0,0,0 }, 1e-4)
    end)

    test(':setPose', function()
      matrix = lovr.math.newMat4():setPose(7,8,9, 1,0,0,1)
      expect({ matrix:unpack() }).to.equal({ 7,8,9, 1,1,1, 1,0,0,1 }, 1e-4)
      matrix:setScale(2)
      matrix:setPose(vector(2, 5, 7), quaternion(3, 1, 0, 0))
      expect({ matrix:unpack() }).to.equal({ 2,5,7, 2,2,2, 3,1,0,0 }, 1e-4)
    end)
  end)

  test('random', function()
    lovr.math.setRandomSeed(7)
    local a = lovr.math.random()
    local b = lovr.math.random()

    lovr.math.setRandomSeed(7)
    expect(lovr.math.random()).to.equal(a)
    expect(lovr.math.random()).to.equal(b)
  end)

  test('randomNormal', function()
    lovr.math.setRandomSeed(7)
    local a = lovr.math.randomNormal()
    local b = lovr.math.randomNormal()

    lovr.math.setRandomSeed(7)
    expect(lovr.math.randomNormal()).to.equal(a)
    expect(lovr.math.randomNormal()).to.equal(b)
  end)


  group('quaternion', function()
    test("angleaxis", function()
      expect({ quaternion(1.2, 1, 0, 0):unpack() }).to.equal({ .564642, 0.000000, 0.000000, 0.825336 }, 1e-4)
      expect({ quaternion(1.2, 0, 0, 0):unpack() }).to.equal({ 0, 0, 0, 1 }, 1e-4)
    end)
  end)
end)
