group('data', function()
  group('Blob', function()
    test(':getName', function()
      -- Test that Blob copies its name instead of relying on Lua string staying live
      blob = lovr.data.newBlob('foo', 'b' .. 'ar')
      collectgarbage()
      expect(blob:getName()).to.equal('b' .. 'ar')
    end)

    test(':set* byte range', function()
      blob = lovr.data.newBlob(1)
      expect(function() blob:setU8(-10, 7) end).to.fail()
      expect(function() blob:setU8(10, 7) end).to.fail()
      expect(function() blob:setF32(0, 7) end).to.fail()
    end)

    test(':setI8', function()
      blob = lovr.data.newBlob(16)
      for i = 1, 16 do blob:setI8(i - 1, i - 8) end
      for i = 1, 16 do expect(blob:getI8(i - 1)).to.equal(i - 8) end
    end)

    test(':setU8', function()
      blob = lovr.data.newBlob(16)
      for i = 1, 16 do blob:setU8(i - 1, i + 150) end
      for i = 1, 16 do expect(blob:getU8(i - 1)).to.equal(i + 150) end
    end)

    test(':setI16', function()
      blob = lovr.data.newBlob(4)
      blob:setI16(0, -5000, 5000)
      expect(blob:getI16(0)).to.equal(-5000)
      expect(blob:getI16(2)).to.equal(5000)
    end)

    test(':setU16', function()
      blob = lovr.data.newBlob(6)
      blob:setU16(0, 0, 1, 60000)
      expect(blob:getU16()).to.equal(0)
      expect({ blob:getU16(2, 2) }).to.equal({ 1, 60000 })
    end)

    test(':setI32', function()
      blob = lovr.data.newBlob(8)
      blob:setI32(4, -12345678)
      expect(blob:getI32(4)).to.equal(-12345678)
    end)

    test(':setU32', function()
      blob = lovr.data.newBlob(4)
      blob:setU32(0, 0xaabbccdd)
      expect(blob:getU32()).to.equal(0xaabbccdd)
      expect(blob:getU8(0)).to.equal(0xdd)
      expect(blob:getU8(1)).to.equal(0xcc)
      expect(blob:getU8(2)).to.equal(0xbb)
      expect(blob:getU8(3)).to.equal(0xaa)
    end)

    test(':setF32', function()
      blob = lovr.data.newBlob(12)
      blob:setF32(0, 1, -1000, 1000000)
      expect({ blob:getF32(0, 3) }).to.equal({ 1, -1000, 1000000 })
    end)

    test(':setF64', function()
      blob = lovr.data.newBlob(8)
      blob:setF64(0, 2 ^ 53)
      expect(blob:getF64(0)).to.equal(2 ^ 53)
    end)

    test('.newBlobView', function ()
      local blob = lovr.data.newBlob(4)
      blob:setU8(0, 0, 1, 2, 3)
      expect(function () lovr.data.newBlobView(blob, -1) end).to.fail()
      expect(function () lovr.data.newBlobView(blob, 1, 4) end).to.fail()
      local blobView = lovr.data.newBlobView(blob, 1, 2, 'name')
      expect(blobView:getU8(0)).to.equal(1)
      expect(blobView:getU8(1)).to.equal(2)
      expect(function () blobView:getU8(2) end).to.fail()
      expect(blobView:getName()).to.equal('name')
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

      -- f16
      image = lovr.data.newImage(4, 4, 'rg16f')
      image:setPixel(0, 0, 1, 2, 3, 4)
      image:setPixel(3, 3, 9, 8, 7, 6)
      expect({ image:getPixel(0, 0) }).to.equal({ 1, 2, 0, 1 })
      expect({ image:getPixel(3, 3) }).to.equal({ 9, 8, 0, 1 })
    end)
  end)

  group('ModelData', function()
    local blob = lovr.data.newBlob([[{
      "asset": { "version": "2.0" },
      "scene": 0,
      "scenes": [{ "nodes": [0] }],
      "nodes": [{ "mesh": 0 }],
      "meshes": [
        {
          "primitives": [{ "attributes": { "POSITION": 0 } }]
        }
      ],
      "buffers": [
        {
          "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA",
          "byteLength": 36
        }
      ],
      "bufferViews": [
        {
          "buffer": 0,
          "byteOffset": 0,
          "byteLength": 36,
          "target": 34962
        }
      ],
      "accessors": [
        {
          "bufferView": 0,
          "byteOffset": 0,
          "componentType": 5126,
          "count": 3,
          "type": "VEC3",
          "max": [1, 1, 0],
          "min": [0, 0, 0]
        }
      ]
    }]])

    local model = lovr.data.newModelData(blob)

    test('getMetadata', function()
      expect(model:getMetadata()).to.equal(blob:getString())
    end)

    test('nodes', function()
      expect(model:getRootNode()).to.equal(1)
      expect(model:getNodeCount()).to.equal(1)
      expect(model:getNodeName(1)).to.equal(nil)
      expect(model:getNodeChild(1)).to.equal(nil)
      expect(model:getNodeSibling(1)).to.equal(nil)
      expect(model:getNodeParent(1)).to.equal(nil)
      expect({ model:getNodeTransform(1) }).to.equal({ 0, 0, 0, 1, 1, 1, 0, 0, 0, 0 })
      expect(model:getNodeMesh(1)).to.equal(1)
      expect(model:getNodeSkin(1)).to.equal(nil)
      expect(function() model:getNodeChild(2) end).to.fail()
    end)

    test('meshes', function()
      expect(model:getMeshCount()).to.equal(1)
      expect(model:getMeshBlendShapeCount(1)).to.equal(0)
      expect(model:getMeshVertexCount(1)).to.equal(3)
      expect(model:getMeshIndexCount(1)).to.equal(0)
      expect({ model:getMeshVertex(1, 1) }).to.equal({ 0, 0, 0; 0, 0, 0; 0, 0; 0, 0; 255, 255, 255, 255; 0, 0, 0 })
      expect({ model:getMeshVertex(1, 2) }).to.equal({ 1, 0, 0; 0, 0, 0; 0, 0; 0, 0; 255, 255, 255, 255; 0, 0, 0 })
      expect({ model:getMeshVertex(1, 3) }).to.equal({ 0, 1, 0; 0, 0, 0; 0, 0; 0, 0; 255, 255, 255, 255; 0, 0, 0 })
      expect(model:getMeshPartCount(1)).to.equal(1)
      expect(model:getMeshDrawMode(1)).to.equal('triangles')
      expect(model:getMeshDrawRange(1)).to.equal(1, 3)
      expect(model:getMeshMaterial(1)).to.equal(nil)
    end)

    test('bounds', function()
      expect(model:getWidth()).to.equal(1)
      expect(model:getHeight()).to.equal(1)
      expect(model:getDepth()).to.equal(0)
      expect({ model:getDimensions() }).to.equal({ 1, 1, 0 })
      expect({ model:getCenter() }).to.equal({ .5, .5, 0 })
      expect({ model:getBoundingBox() }).to.equal({ 0, 1, 0, 1, 0, 0 })
    end)
  end)
end)
