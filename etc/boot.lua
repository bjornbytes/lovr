lovr = require 'lovr'

local lovr = lovr

local conf = {
  version = '0.17.0',
  identity = 'default',
  saveprecedence = true,
  modules = {
    audio = true,
    data = true,
    event = true,
    graphics = true,
    headset = true,
    math = true,
    physics = true,
    system = true,
    thread = true,
    timer = true
  },
  audio = {
    start = true,
    spatializer = nil
  },
  graphics = {
    debug = false,
    vsync = true,
    stencil = false,
    antialias = true,
    shadercache = true
  },
  headset = {
    drivers = { 'openxr', 'webxr', 'desktop' },
    supersample = false,
    seated = false,
    stencil = false,
    antialias = true,
    submitdepth = true,
    overlay = false
  },
  math = {
    globals = true
  },
  window = {
    width = 720,
    height = 800,
    fullscreen = false,
    resizable = false,
    title = 'LÖVR',
    icon = nil
  }
}

function lovr.boot()
  lovr.filesystem = require('lovr.filesystem')

  local bundle, root = lovr.filesystem.getBundlePath()
  local fused = lovr.filesystem.mount(bundle, nil, true, root)
  local cli = lovr.filesystem.isFile('arg.lua') and assert(pcall(require, 'arg')) and lovr.arg and lovr.arg(arg)
  local source, main = bundle, 'main.lua'

  if not fused then
    if arg[1] and not arg[1]:match('^%-') then
      for i = 0, #arg do
        arg[i - 1], arg[i] = arg[i], nil
      end
    else
      return function()
        print(table.concat({
          'usage: lovr <source>',
          '<source> can be a Lua file, a folder with a main.lua file, or a zip archive'
        }, '\n'))
        return 1
      end
    end
  end

  if cli or not fused then
    if arg[0] and arg[0]:match('[^/\\]+%.lua$') then
      source = arg[0]:match('[/\\]') and arg[0]:match('(.+)[/\\][^/\\]+$') or '.'
      main = arg[0]:match('[^/\\]+%.lua$')
    else
      source = arg[0]
    end
  end

  local ok, failure

  if source ~= bundle and not lovr.filesystem.mount(source) then
    failure = ('Couldn\'t find path %q, or it wasn\'t a valid zip archive.'):format(source)
  elseif not lovr.filesystem.isFile(main) then
    failure = ('No %s file found in %q.'):format(main, source:match('[^/\\]+[/\\]?$'))
  else
    lovr.filesystem.setSource(source)
    if lovr.filesystem.isFile('conf.lua') then ok, failure = pcall(require, 'conf') end
    if ok and lovr.conf then ok, failure = pcall(lovr.conf, conf) end
  end

  lovr._setConf(conf)
  lovr.filesystem.setIdentity(conf.identity, conf.saveprecedence)

  if not failure and cli then ok, failure = pcall(cli, conf) end

  for module in pairs(conf.modules) do
    if conf.modules[module] then
      local ok, result = pcall(require, 'lovr.' .. module)
      if not ok then
        lovr.log('warn', string.format('Could not load module %q: %s', module, result))
      else
        lovr[module] = result
      end
    end
  end

  if lovr.graphics then
    lovr.graphics.initialize()
  end

  if lovr.system and conf.window then
    lovr.system.openWindow(conf.window)
  end

  if lovr.headset then
    lovr.headset.start()
  end

  if failure then
    error(failure)
  end

  require(main:sub(1, -5))

  return lovr.run()
end

function lovr.run()
  if lovr.timer then lovr.timer.step() end
  if lovr.load then lovr.load(arg) end
  return function()
    if lovr.system then lovr.system.pollEvents() end
    if lovr.event then
      for name, a, b, c, d in lovr.event.poll() do
        if name == 'restart' then return 'restart', lovr.restart and lovr.restart()
        elseif name == 'quit' and (not lovr.quit or not lovr.quit(a)) then return a or 0
        elseif name ~= 'quit' and lovr.handlers[name] then lovr.handlers[name](a, b, c, d) end
      end
    end
    local dt = 0
    if lovr.timer then dt = lovr.timer.step() end
    if lovr.headset then dt = lovr.headset.update() end
    if lovr.update then lovr.update(dt) end
    if lovr.graphics then
      local headset = lovr.headset and lovr.headset.getPass()
      if headset and (not lovr.draw or lovr.draw(headset)) then headset = nil end
      local window = lovr.graphics.getWindowPass()
      if window and (not lovr.mirror or lovr.mirror(window)) then window = nil end
      lovr.graphics.submit(headset, window)
      lovr.graphics.present()
    end
    if lovr.headset then lovr.headset.submit() end
    if lovr.math then lovr.math.drain() end
  end
end

function lovr.mirror(pass)
  if lovr.headset then
    local texture = lovr.headset.getTexture()
    if texture then
      pass:fill(texture)
    end
  else
    return lovr.draw and lovr.draw(pass)
  end
end

local function formatTraceback(s)
  return s:gsub('\n[^\n]+$', ''):gsub('\t', ''):gsub('stack traceback:', '\nStack:\n')
end

function lovr.errhand(message)
  message = 'Error:\n\n' .. tostring(message) .. formatTraceback(debug.traceback('', 4))

  print(message)

  if not lovr.graphics then
    return function() return 1 end
  end

  if lovr.audio then lovr.audio.stop() end

  if not lovr.headset or lovr.headset.getPassthrough() == 'opaque' then
    lovr.graphics.setBackgroundColor(.11, .10, .14)
  else
    lovr.graphics.setBackgroundColor(0, 0, 0, 0)
  end

  local font = lovr.graphics.getDefaultFont()

  return function()
    lovr.system.pollEvents()

    for name, a in lovr.event.poll() do
      if name == 'quit' then return a or 1
      elseif name == 'restart' then return 'restart', lovr.restart and lovr.restart()
      elseif name == 'keypressed' and a == 'f5' then lovr.event.restart()
      elseif name == 'keypressed' and a == 'escape' then lovr.event.quit() end
    end

    if lovr.headset and lovr.headset.getDriver() ~= 'desktop' then
      lovr.headset.update()
      local pass = lovr.headset.getPass()
      if pass then
        font:setPixelDensity()

        local scale = .35
        local font = lovr.graphics.getDefaultFont()
        local wrap = .7 * font:getPixelDensity()
        local lines = font:getLines(message, wrap)
        local width = math.min(font:getWidth(message), wrap) * scale
        local height = .8 + #lines * font:getHeight() * scale
        local x = -width / 2
        local y = math.min(height / 2, 10)
        local z = -10

        pass:setColor(.95, .95, .95)
        pass:text(message, x, y, z, scale, 0, 0, 0, 0, wrap, 'left', 'top')

        lovr.graphics.submit(pass)
        lovr.headset.submit()
      end
    end

    if lovr.system.isWindowOpen() then
      local pass = lovr.graphics.getWindowPass()
      if pass then
        local w, h = lovr.system.getWindowDimensions()
        pass:setProjection(1, lovr.math.mat4():orthographic(0, w, 0, h, -1, 1))
        font:setPixelDensity(1)

        local scale = .6
        local wrap = w * .8 / scale
        local width = math.min(font:getWidth(message), wrap) * scale
        local x = w / 2 - width / 2

        pass:setColor(.95, .95, .95)
        pass:text(message, x, h / 2, 0, scale, 0, 0, 0, 0, wrap, 'left', 'middle')

        lovr.graphics.submit(pass)
        lovr.graphics.present()
      end
    end

    lovr.math.drain()
  end
end

function lovr.threaderror(thread, err)
  error('Thread error\n\n' .. err, 0)
end

function lovr.log(message, level, tag)
  message = message:gsub('\n$', '')
  print(message)
end

lovr.handlers = setmetatable({}, { __index = lovr })

return coroutine.create(function()
  local function onerror(...)
    onerror = function(...)
      print('Error:\n\n' .. tostring(...) .. formatTraceback(debug.traceback('', 3)))
      return function() return 1 end
    end
    return lovr.errhand(...) or function() return 1 end
  end

  local thread = select(2, xpcall(lovr.boot, onerror))

  while true do
    local ok, result, cookie = xpcall(thread, onerror)
    if not ok then thread = result
    elseif result then return result, cookie end
    coroutine.yield()
  end
end)
