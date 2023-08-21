lovr = require 'lovr'

-- Note: Cannot be overloaded
function lovr.boot()
  local conf = {
    version = '0.16.0',
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
      offset = 1.7,
      stencil = false,
      antialias = true,
      submitdepth = true,
      overlay = 0
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

  lovr.filesystem = require('lovr.filesystem')
  local main = arg[0] and arg[0]:match('[^\\/]-%.lua$') or 'main.lua'
  local hasConf, hasMain = lovr.filesystem.isFile('conf.lua'), lovr.filesystem.isFile(main)
  if not lovr.filesystem.getSource() or not (hasConf or hasMain) then require('nogame') end

  -- Shift args up in fused mode, instead of consuming one for the source path
  if lovr.filesystem.isFused() then
    for i = 1, #arg + 1 do
      arg[i] = arg[i - 1]
    end
    arg[0] = lovr.filesystem.getSource()
  end

  local confOk, confError = true
  if hasConf then confOk, confError = pcall(require, 'conf') end
  if confOk and lovr.conf then confOk, confError = pcall(lovr.conf, conf) end

  conf.graphics.debug = arg['--graphics-debug'] or conf.graphics.debug

  lovr._setConf(conf)
  lovr.filesystem.setIdentity(conf.identity, conf.saveprecedence)

  for module in pairs(conf.modules) do
    if conf.modules[module] then
      local ok, result = pcall(require, 'lovr.' .. module)
      if not ok then
        print(string.format('Warning: Could not load module %q: %s', module, result))
      else
        lovr[module] = result
      end
    end
  end

  if lovr.system and conf.window then
    lovr.system.openWindow(conf.window)
  end

  if lovr.graphics then
    lovr.graphics.initialize()
  end

  if lovr.headset then
    lovr.headset.start()
  end

  lovr.handlers = setmetatable({}, { __index = lovr })
  if not confOk then error(confError) end
  if hasMain then require(main:gsub('%.lua$', '')) end
  return lovr.run()
end

function lovr.run()
  if lovr.timer then lovr.timer.step() end
  if lovr.load then lovr.load(arg) end
  return function()
    if lovr.system then lovr.system.pollEvents() end
    if lovr.event then
      for name, a, b, c, d in lovr.event.poll() do
        if name == 'restart' then
          local cookie = lovr.restart and lovr.restart()
          return 'restart', cookie
        elseif name == 'quit' and (not lovr.quit or not lovr.quit(a)) then
          return a or 0
        end
        if lovr.handlers[name] then lovr.handlers[name](a, b, c, d) end
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

  if not lovr.graphics or not lovr.graphics.isInitialized() then
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

return function()
  local errored = false

  local function onerror(...)
    if not errored then
      errored = true -- Ensure errhand is only called once
      return lovr.errhand(...) or function() return 1 end
    else
      local message = tostring(...) .. formatTraceback(debug.traceback('', 2))
      print('Error occurred while trying to display another error:\n' .. message)
      return function() return 1 end
    end
  end

  -- thread will be either lovr.run's step function, or the result of errhand
  local _, thread = xpcall(lovr.boot, onerror)

  while true do
    local ok, result, cookie = xpcall(thread, onerror)

    -- If step function returned something, exit coroutine and return to C
    if result and ok then
      return result, cookie
    elseif not ok then -- Switch to errhand loop
      thread = result
      if type(thread) ~= 'function' then
        print('Error occurred while trying to display another error:\n' .. tostring(result))
        return 1
      end
    end

    coroutine.yield()
  end
end
