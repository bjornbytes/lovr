group('thread', function()
  group('Thread', function()
    test(':start', function()
      local thread = lovr.thread.newThread([[
        require('lovr.data')
        assert((...):type() == 'Blob')
      ]])

      thread:start(lovr.data.newBlob(1))
      thread:wait()
    end)
  end)

  group('Channel', function()
    test('push/pop', function()
      local channel = lovr.thread.getChannel('test')
      local data = { 123, a = 1, b = channel, c = { 321, a = channel }, d = {} }
      channel:push(data)
      expect(channel:pop()).to.equal(data)

      local t = { 123, a = 1 }
      t.t = t
      expect(function() channel:push(t) end).to.fail()
    end)
  end)
end)
