group('graphics', function()
  group('Buffer', function()
    local buffer, data

    test('format: none', function()
      buffer = lovr.graphics.newBuffer(16)
      expect(buffer:getSize()).to.be(16)
      expect(buffer:getLength()).to.be(nil)
      expect(buffer:getStride()).to.be(nil)
      expect(buffer:getFormat()).to.be(nil)
    end)

    test('format: scalar', function()
      buffer = lovr.graphics.newBuffer('int')
      expect(buffer:getSize()).to.be(4)
      expect(buffer:getLength()).to.be(0)
      expect(buffer:getStride()).to.be(4)
      buffer:setData(-3)
      expect(buffer:getData()).to.be(-3)
    end)

    test('format: vector', function()
      buffer = lovr.graphics.newBuffer('vec3', vec3(1, 2, 3))
      expect(buffer:getSize()).to.be(12)
      expect(buffer:getLength()).to.be(0)
      expect(buffer:getStride()).to.be(12)
      expect({ buffer:getData() }).to.equal({ 1, 2, 3 })

      buffer:setData(vec3(4, 5, 6))
      expect({ buffer:getData() }).to.equal({ 4, 5, 6 })

      buffer:setData({ 7, 8, 9 })
      x, y, z = buffer:getData()
      expect({ buffer:getData() }).to.equal({ 7, 8, 9 })
    end)

    test('format: vector array', function()
      buffer = lovr.graphics.newBuffer('vec3', 2)
      expect(buffer:getSize()).to.equal(24)
      expect(buffer:getLength()).to.equal(2)
      expect(buffer:getStride()).to.equal(12)
      buffer:setData({ vec3(1, 2, 3), vec3(4, 5, 6) })
      expect(buffer:getData()).to.equal({ 1, 2, 3, 4, 5, 6 })
    end)

    test('format: scalar array (single)', function()
      buffer = lovr.graphics.newBuffer('int', 1)
      expect(buffer:getSize()).to.be(4)
      expect(buffer:getLength()).to.be(1)
      expect(buffer:getStride()).to.be(4)
      buffer:setData({ -5 })
      expect(buffer:getData()).to.equal({ -5 })
    end)

    test('format: scalar array', function()
      buffer = lovr.graphics.newBuffer('float', 3)
      expect(buffer:getSize()).to.be(12)
      expect(buffer:getLength()).to.be(3)
      expect(buffer:getStride()).to.be(4)
      buffer:setData({ .5, .25, .125 })
      expect(buffer:getData()).to.equal({ .5, .25, .125 })
    end)

    test('format: anonymous struct', function()
      buffer = lovr.graphics.newBuffer({ 'vec2', 'vec3', 'vec4' }, 0)
      expect(buffer:getSize()).to.be(36)
      expect(buffer:getLength()).to.be(0)
      expect(buffer:getStride()).to.be(36)

      buffer:setData({ 1, 2; 3, 4, 5; 6, 7, 8, 9 })
      expect(buffer:getData()).to.equal({ { 1, 2 }, { 3, 4, 5 }, { 6, 7, 8, 9 } })

      buffer:setData({ { 9, 8 }, { 7, 6, 5 }, { 4, 3, 2, 1 } })
      expect(buffer:getData()).to.equal({ { 9, 8 }, { 7, 6, 5 }, { 4, 3, 2, 1 } })

      buffer:setData({ vec2(1, 2), vec3(3, 4, 5), vec4(6, 7, 8, 9) })
      expect(buffer:getData()).to.equal({ { 1, 2 }, { 3, 4, 5 }, { 6, 7, 8, 9 } })
    end)

    test('format: anonymous struct array', function()
      buffer = lovr.graphics.newBuffer({ 'vec2', 'vec3' }, 2)
      expect(buffer:getSize()).to.equal(40)
      expect(buffer:getLength()).to.equal(2)
      expect(buffer:getStride()).to.equal(20)

      buffer:setData({ { 1, 2, 3, 4, 5 }, { 5, 4, 3, 2, 1 } })
      expect(buffer:getData()).to.equal({ { { 1, 2 }, { 3, 4, 5 } }, { { 5, 4 }, { 3, 2, 1 } } })

      buffer:setData({ { { 4, 4 }, { 3, 3, 3 } }, { { 2, 2 }, { 1, 1, 1 } }, -0 })
      expect(buffer:getData()).to.equal({ { { 4, 4 }, { 3, 3, 3 } }, { { 2, 2 }, { 1, 1, 1 } } })

      buffer:setData({ { vec2(7), vec3(6) }, { vec2(4), vec3(5) } })
      expect(buffer:getData()).to.equal({ { { 7, 7 }, { 6, 6, 6 } }, { { 4, 4 }, { 5, 5, 5 } } })
    end)

    test('format: struct', function()
      buffer = lovr.graphics.newBuffer({ { 'a', 'float' }, { 'b', 'vec2' } })

      buffer:setData({ a = 7, b = { 1, 2 } })
      expect(buffer:getData()).to.equal({ a = 7, b = { 1, 2 } })

      buffer:setData({ a = 8, b = vec2(3, 4) })
      expect(buffer:getData()).to.equal({ a = 8, b = { 3, 4 } })

      buffer:setData({ 1, 2, 3 })
      expect(buffer:getData()).to.equal({ a = 1, b = { 2, 3 } })

      buffer:setData({ 4, { 5, 6 } })
      expect(buffer:getData()).to.equal({ a = 4, b = { 5, 6 } })

      buffer:setData({ 7, vec2(8, 9) })
      expect(buffer:getData()).to.equal({ a = 7, b = { 8, 9 } })

      buffer:setData({ 10, b = vec2(11, 12) })
      expect(buffer:getData()).to.equal({ a = 10, b = { 11, 12 } })
    end)

    test('format: struct array', function()
      buffer = lovr.graphics.newBuffer({ { 'a', 'float' }, { 'b', 'vec2' } }, {
        { a = 1, b = { 2, 3 } },
        { a = 4, b = { 5, 6 } }
      })

      expect(buffer:getData()).to.equal({ { a = 1, b = { 2, 3 } }, { a = 4, b = { 5, 6 } } })

      buffer:setData({
        { 9, 8, 7 },
        { a = 6, b = vec2(5, 4) }
      })

      expect(buffer:getData()).to.equal({ { a = 9, b = { 8, 7 } }, { a = 6, b = { 5, 4 } } })
      expect(function() buffer:setData({ 1, 2, 3, 4, 5, 6 }) end).to.fail()
    end)

    test('format: nested array of structs', function()
      buffer = lovr.graphics.newBuffer({
        { 'list', { { 'pos', 'vec3' }, { 'size', 'float' } }, 3 },
        { 'count', 'int' }
      })

      buffer:setData({
        list = {
          { vec3(1, 1, 1), 2 },
          { 3, 3, 3, 4 },
          { pos = { 5, 5, 5 }, size = 6 }
        },
        count = 101
      })

      expect(buffer:getData()).to.equal({
        list = {
          { pos = { 1, 1, 1 }, size = 2 },
          { pos = { 3, 3, 3 }, size = 4 },
          { pos = { 5, 5, 5 }, size = 6 }
        },
        count = 101
      })
    end)

    test('format: nested structs', function()
      buffer = lovr.graphics.newBuffer({
        { 'a', {
          { 'b', {
            { 'c', 'vec2' }
          }}
        }}
      })
      expect(buffer:getSize()).to.be(8)
      expect(buffer:getLength()).to.be(0)
      expect(buffer:getStride()).to.be(8)
      buffer:setData({ a = { b = { c = { 1.5, 2.5 } } } })
      expect(buffer:getData()).to.equal({ a = { b = { c = { 1.5, 2.5 } } } })
    end)

    test('format: layout std140', function()
      buffer = lovr.graphics.newBuffer({ 'float', layout = 'std140' })
      expect(buffer:getStride()).to.be(16)
    end)

    test('format: layout std430', function()
      buffer = lovr.graphics.newBuffer({ 'vec3', layout = 'std430' })
      expect(buffer:getStride()).to.be(16)
    end)

    test('format: blob length', function()
      buffer = lovr.graphics.newBuffer('un10x3', lovr.data.newBlob(16))
      expect(buffer:getLength()).to.be(4)
      buffer = lovr.graphics.newBuffer('un10x3', lovr.data.newBlob(15))
      expect(buffer:getLength()).to.be(3)
    end)

    test('format: runtime-sized array', function()
      shader = lovr.graphics.newShader('buffer Buffer { uint count; int data[]; }; void lovrmain(){}\n')
      format, length = shader:getBufferFormat('Buffer')
      expect(length).to.equal(nil)
      expect(#format).to.equal(2)
      expect(format[1]).to.equal({ name = 'count', type = 'u32', offset = 0 })
      expect(format[2]).to.equal({ name = 'data', type = 'i32', offset = 4, length = -1, stride = 4 })
      buffer = lovr.graphics.newBuffer(shader:getBufferFormat('Buffer'), { count = 3, data = { 4, 5, 6 } })
      expect(buffer:getSize()).to.equal(16)
      expect(buffer:getLength()).to.equal(0)
      expect(buffer:getFormat()[2].length).to.equal(3)
      expect(buffer:getData()).to.equal({ count = 3, data = { 4, 5, 6 } })
    end)

    test('format: manual runtime-sized array', function()
      buffer = lovr.graphics.newBuffer({ { name = 'a', length = -1, type = 'vec3' }, layout = 'std140' })
      expect(buffer:getLength()).to.equal(0)
      expect(buffer:getStride()).to.equal(16)
      expect(buffer:getSize()).to.equal(16)
      expect(buffer:getFormat()).to.equal({ { name = 'a', length = 1, type = 'f32x3', offset = 0, stride = 16 } })

      buffer = lovr.graphics.newBuffer({
        { name = 'a', length = -1, type = 'vec3' }, layout = 'std140' },
        { a = { { 1, 2, 3 }, { 4, 5, 6 } }
      })
      expect(buffer:getStride()).to.equal(32)

      expect(function()
        lovr.graphics.newBuffer({
          { name = 'a', length = -1, type = 'f32' },
          { name = 'b', length = -1, type = 'f32' }
        })
      end).to.fail()
    end)

    test(':setData offset', function()
      buffer = lovr.graphics.newBuffer('int', { 1, 2, 3 })
      expect(buffer:getSize()).to.be(12)
      expect(buffer:getLength()).to.be(3)
      expect(buffer:getStride()).to.be(4)
      expect(buffer:getData()).to.equal({ 1, 2, 3 })

      buffer:setData({ 8, 9 }, 2)
      expect(buffer:getData()).to.equal({ 1, 8, 9 })

      buffer:setData({ -1, -2 }, 3, 2, 1)
      expect(buffer:getData()).to.equal({ 1, 8, -2 })
    end)

    test(':setData with Blob', function()
      buffer = lovr.graphics.newBuffer('int')
      blob = lovr.data.newBlob(4)
      buffer:setData(blob)
    end)

    test(':clear', function()
      buffer = lovr.graphics.newBuffer('int', { 1, 2, 3 })
      buffer:clear()
      expect(buffer:getData()).to.equal({ 0, 0, 0 })
    end)

    local ok, ffi = pcall(require, 'ffi')
    if ok and ffi then
      test(':mapData FFI', function()
        buffer = lovr.graphics.newBuffer('float')
        ffi.cast('float*', buffer:mapData())[0] = 7
        expect(buffer:getData()).to.equal(7)
      end)
    end

    test('Pass:send uniform formats', function()
      shader = lovr.graphics.newShader([[
        struct S { int a, b, c; };

        uniform float x;
        uniform vec2 y;
        uniform float z[3];
        uniform vec2 w[2];
        uniform S s;
        uniform S t[2];

        void lovrmain() {}
      ]])

      pass = lovr.graphics.newPass()
      pass:setShader(shader)
      pass:send('x', 7)
      pass:send('y', { 3, 4 })
      pass:send('y', vec2(3, 4))
      pass:send('z', { 1, 2, 3 })
      pass:send('w', { 1, 2, 3, 4 })
      pass:send('w', { vec2(1, 2), vec2(3, 4) })
      pass:send('w', { { 1, 2 }, { 3, 4 } })
      pass:send('s', { a = 1, b = 2, c = 3 })
      pass:send('t', { { a = 1, b = 2, c = 3 }, { a = 4, b = 5, c = 6 } })
    end)

    test('Shader:getBufferFormat', function()
      shader = lovr.graphics.newShader([[
        struct S { float x, y; };

        buffer Buffer1 { float x; } a;
        buffer Buffer2 { float x, y; } b;
        buffer Buffer3 { float x[4]; } c;
        buffer Buffer4 { float x[4], y[4]; } d;
        buffer Buffer5 { S s; } e;
        buffer Buffer6 { S s[2]; } f;
        buffer Buffer7 { float x; S s[]; } g;

        void lovrmain() {}
      ]])

      local format, length = shader:getBufferFormat('Buffer1')
      expect(format).to.equal({{ type = 'f32', name = 'x', offset = 0 }})
      expect(length).to.be(nil)

      local format, length = shader:getBufferFormat('Buffer2')
      expect(format).to.equal({
        { type = 'f32', name = 'x', offset = 0 },
        { type = 'f32', name = 'y', offset = 4 }
      })
      expect(length).to.be(nil)

      local format, length = shader:getBufferFormat('Buffer3')
      expect(format).to.equal({ 'f32', stride = 4 })
      expect(length).to.be(4)

      local format, length = shader:getBufferFormat('Buffer4')
      expect(format).to.equal({
        { type = 'f32', name = 'x', length = 4, stride = 4, offset = 0 },
        { type = 'f32', name = 'y', length = 4, stride = 4, offset = 16 },
      })

      local format, length = shader:getBufferFormat('Buffer5')
      expect(format).to.equal({
        { name = 's', offset = 0, type = {
          { name = 'x', offset = 0, type = 'f32' },
          { name = 'y', offset = 4, type = 'f32' }
        }}
      })
      expect(length).to.be(nil)

      local format, length = shader:getBufferFormat('Buffer6')
      expect(format).to.equal({
        { name = 'x', offset = 0, type = 'f32' },
        { name = 'y', offset = 4, type = 'f32' },
        stride = 8
      })
      expect(length).to.be(2)

      local format, length = shader:getBufferFormat('Buffer7')
      expect(format).to.equal({
        { name = 'x', offset = 0, type = 'f32' },
        { name = 's', offset = 4, stride = 8, length = -1, type = {
          { name = 'x', offset = 0, type = 'f32' },
          { name = 'y', offset = 4, type = 'f32' },
        }}
      })
      expect(length).to.be(nil)
    end)
  end)

  group('Font', function()
    test('newFont(Rasterizer)', function()
      do lovr.graphics.newFont(lovr.data.newRasterizer(42)) end
      collectgarbage()
      collectgarbage()
    end)

    test(':getLines', function()
      local font = lovr.graphics.getDefaultFont()
      local lines = font:getLines({ 0xff0000, 'hello ', 0x0000ff, 'world' }, 0)
      expect(lines).to.equal({ 'hello ', 'world' })
    end)
  end)

  group('Mesh', function()
    group('.newMesh', function()
      test('MeshStorage=gpu initial vertex upload', function()
        mesh = lovr.graphics.newMesh({{ 'x', 'int' }}, {{42}}, 'gpu')
        expect(mesh:getVertexCount()).to.equal(1)
        expect(mesh:getVertexBuffer():getData()).to.equal({{ x = 42 }})
      end)
    end)

    test(':getVertices', function()
      local vertices = {
        { 1, 2, 3, 4, 5, 6, 7, 8 },
        { 8, 7, 6, 5, 4, 3, 2, 1 }
      }

      mesh = lovr.graphics.newMesh(vertices)
      expect(mesh:getVertices()).to.equal(vertices)
    end)

    test(':setDrawRange', function()
      mesh = lovr.graphics.newMesh({{ 1, 2, 3 }, { 4, 5, 6 }, { 7, 8, 9 }})
      mesh:setDrawRange(1, 3, 2)
      expect({ mesh:getDrawRange() }).to.equal({ 1, 3, 2 })
    end)
  end)

  group('Pass', function()
    test(':getDimensions', function()
      pass = lovr.graphics.newPass()
      expect(pass:getWidth()).to.equal(0)
      expect(pass:getHeight()).to.equal(0)
      expect({ pass:getDimensions() }).to.equal({ 0, 0 })

      pass:setCanvas(lovr.graphics.newTexture(1, 1))
      expect(pass:getWidth()).to.equal(1)
      expect(pass:getHeight()).to.equal(1)
      expect({ pass:getDimensions() }).to.equal({ 1, 1 })

      pass:setCanvas()
      expect(pass:getWidth()).to.equal(0)
      expect(pass:getHeight()).to.equal(0)
      expect({ pass:getDimensions() }).to.equal({ 0, 0 })
    end)

    test(':setCanvas', function()
      -- depth only
      texture = lovr.graphics.newTexture(100, 100, { format = 'd32f' })
      pass = lovr.graphics.newPass({ depth = texture })
      lovr.graphics.submit(pass)
    end)

    test(':send', function()
      -- First draw has uniforms, second draw does not, and first draw is culled
      shader1 = lovr.graphics.newShader('unlit', [[
        Constants { vec4 color; };
        vec4 lovrmain() { return color; }
      ]])

      shader2 = lovr.graphics.newShader('unlit', [[
        vec4 lovrmain() { return vec4(1.); }
      ]])

      texture = lovr.graphics.newTexture(1, 1)
      pass = lovr.graphics.newPass(texture)
      pass:setViewCull(true)
      pass:setShader(shader1)
      pass:sphere(0, 0, 10)
      pass:setShader(shader2)
      pass:sphere(0, 0, -10)
      lovr.graphics.submit(pass)

      -- First dispatch has uniforms, second dispatch does not
      shader1 = lovr.graphics.newShader([[
        Constants { vec4 color; };
        void lovrmain() { }
      ]])

      shader2 = lovr.graphics.newShader([[
        void lovrmain() { }
      ]])

      pass = lovr.graphics.newPass()
      pass:setShader(shader1)
      pass:compute()
      pass:setShader(shader2)
      pass:compute()
      lovr.graphics.submit(pass)

      -- Test that second draw with bigger uniform buffer is able to use its trailing uniforms
      shader1 = lovr.graphics.newShader('fill', [[
        uniform vec4 color1;
        vec4 lovrmain() { return color1; }
      ]])

      shader2 = lovr.graphics.newShader('fill', [[
        uniform vec4 color1;
        uniform vec4 color2;
        vec4 lovrmain() { return color2; }
      ]])

      texture = lovr.graphics.newTexture(1, 1, { usage = { 'render', 'transfer' } })
      pass = lovr.graphics.newPass(texture)

      pass:setShader(shader1)
      pass:send('color1', vec4(1, 0, 0, 1))
      pass:fill()

      pass:setShader(shader2)
      pass:send('color2', vec4(0, 0, 1, 1))
      pass:fill()

      lovr.graphics.submit(pass)
      image = texture:getPixels()
      expect({ image:getPixel(0, 0) }).to.equal({ 0, 0, 1, 1 })
    end)
  end)

  group('Shader', function()
    test('lots of flags', function()
      computer = lovr.graphics.newShader([[
        layout(constant_id = 0) const int x = 0;
        layout(constant_id = 1) const int y = 1;
        layout(constant_id = 2) const int z = 2;
        layout(constant_id = 3) const int w = 3;
        buffer Buffer { vec4 v; };
        void lovrmain() { v = vec4(x, y, z, w); }
      ]], {
        flags = {
          x = 4,
          y = 5,
          z = 6,
          w = 7
        }
      })

      buffer = lovr.graphics.newBuffer('vec4')
      pass = lovr.graphics.newPass()
      pass:setShader(computer)
      pass:send('Buffer', buffer)
      pass:compute()
      lovr.graphics.submit(pass)
      expect({ buffer:getData() }).to.equal({ 4, 5, 6, 7 })
    end)

    group(':clone', function()
      test('workgroup size', function()
        shader = lovr.graphics.newShader('layout(local_size_x = 1, local_size_y = 2, local_size_z = 3) in;void lovrmain(){}\n')
        expect({ shader:getWorkgroupSize() }).to.equal({ 1, 2, 3 })
        expect({ shader:clone({}):getWorkgroupSize() }).to.equal({ 1, 2, 3 })
      end)

      test('attributes', function()
        shader = lovr.graphics.newShader('in vec4 x;vec4 lovrmain(){return vec4(1.);}\n', 'unlit')
        expect(shader:hasAttribute('x')).to.equal(true)
        expect(shader:clone({}):hasAttribute('x')).to.equal(true)
      end)
    end)

    test(':hasVariable', function()
      shader = lovr.graphics.newShader([[
        uniform vec3 position;
        vec4 lovrmain() { return vec4(position, 1.); }
      ]], [[
        buffer Params { vec4 color; };
        uniform texture2D image;
        vec4 lovrmain() { return color; }
      ]])

      expect(shader:hasVariable('position')).to.equal(true)
      expect(shader:hasVariable('unknown')).to.equal(false)
      expect(shader:hasVariable('Params')).to.equal(true)
      expect(shader:hasVariable('image')).to.equal(true)
    end)
  end)
end)
