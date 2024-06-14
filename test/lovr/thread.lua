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
end)
