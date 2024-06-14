function lovr.conf(t)
  t.identity = 'test'
  t.modules.graphics = not os.getenv('CI')
  t.window = nil
end
