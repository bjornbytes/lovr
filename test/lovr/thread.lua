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
      channel:push({ 123, a = 1, b = channel, c = { 321, a = channel }, d = {} })
      local r = channel:pop(true)
      assert(r[1] == 123)
      assert(r.a == 1)
      assert(r.b == channel)
      assert(type(r.c) == 'table')
      assert(r.c[1] == 321)
      assert(r.c.a == channel)
      assert(type(r.d) == 'table')
      assert(next(r.d) == nil)


      local t = { 123, a = 1 }
      t.t = t
      local ok, r = pcall(channel.push, channel, t)
      assert(ok == false)
      assert(r:match("depth > 128"))
    end)
  end)
end)
