lovr = require 'lovr'

-- Note: Cannot be overloaded
function lovr.boot()
  local conf = {
    version = '0.15.0',
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
      vsync = false,
      stencil = false,
      antialias = true
    },
    headset = {
      drivers = { 'openxr', 'webxr', 'desktop' },
      supersample = false,
      offset = 1.7,
      stencil = false,
      antialias = true,
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
    lovr.graphics.init()
  end

  if lovr.headset and lovr.graphics and conf.window then
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
    if lovr.event then
      lovr.event.pump()
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
      if lovr.headset then
        local pass = lovr.headset.getPass()
        if pass then
          local skip = lovr.draw and lovr.draw(pass)
          if not skip then lovr.graphics.submit(pass) end
        end
      end
      if lovr.system.isWindowOpen() then
        local pass = lovr.graphics.getWindowPass()
        pass:reset()
        local skip = lovr.mirror(pass)
        if not skip then lovr.graphics.submit(pass) end
      end
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
    else
      return true
    end
  else
    return lovr.draw and lovr.draw(pass)
  end
end

local function formatTraceback(s)
  return s:gsub('\n[^\n]+$', ''):gsub('\t', ''):gsub('stack traceback', '\nStack')
end

function lovr.errhand(message, traceback)
  message = tostring(message)
  message = message .. formatTraceback(traceback or debug.traceback('', 4))
  print('Error:\n' .. message)
  if not lovr.graphics then return function() return 1 end end

  lovr.graphics.submit()
  lovr.graphics.setBackground(.11, .10, .14)

  local scale = .3
  local font = lovr.graphics.getDefaultFont()
  local wrap = .7 * font:getPixelDensity()
  local lines = font:getLines(message, wrap)
  local width = math.min(font:getWidth(message), wrap) * scale
  local height = .8 + #lines * font:getHeight() * scale
  local x = -width / 2
  local y = math.min(height / 2, 10)
  local z = -10
  font:setPixelDensity()

  local function render(pass)
    pass:setColor(.95, .95, .95)
    pass:setBlendMode('alpha')
    pass:text('Error', x, y, z, scale * 1.6, 0, 0, 0, 0, nil, 'left', 'top')
    pass:text(message, x, y - .8, z, scale, 0, 0, 0, 0, wrap, 'left', 'top')
  end

  return function()
    lovr.event.pump()
    for name, a in lovr.event.poll() do
      if name == 'quit' then return a or 1
      elseif name == 'restart' then return 'restart', lovr.restart and lovr.restart()
      elseif name == 'keypressed' and a == 'f5' then lovr.event.restart() end
    end
    local passes = {}
    if lovr.headset then
      lovr.headset.update()
      local texture = lovr.headset.getTexture()
      if texture then
        local pass = lovr.graphics.getPass('render', texture)
        local near, far = lovr.headset.getClipDistance()
        for i = 1, lovr.headset.getViewCount() do
          pass:setViewPose(i, lovr.headset.getViewPose(i))
          local left, right, up, down = lovr.headset.getViewAngles(i)
          pass:setProjection(i, left, right, up, down, near, far)
        end
        render(pass)
        passes[#passes + 1] = pass
      end
    end
    if lovr.system.isWindowOpen() then
      local pass = lovr.graphics.getPass('render', 'window')
      local width, height = lovr.system.getWindowDimensions()
      local projection = lovr.math.mat4():perspective(1.0, width / height, .1, 100)
      pass:setProjection(1, projection)
      render(pass)
      passes[#passes + 1] = pass
    end
    lovr.graphics.submit(passes)
    if lovr.math then lovr.math.drain() end
  end
end

function lovr.threaderror(thread, err)
  error('Thread error\n\n' .. err, 0)
end

function lovr.log(message, level, tag)
  message = message:gsub('\n$', '')
  print(message)
end

-- This splits up the string returned by luax_getstack so it looks like the error message plus the string from
-- debug.traceback(). This includes splitting on the newline before 'stack traceback:' and appending a newline
local function splitOnLabelLine(s, t)
  local at = s:reverse():find(t:reverse())
  if at then
    local slen = #s
    at = (#s - at - #t + 2)
    return s:sub(1, at-2), s:sub(at,slen) .. '\n'
  else
    return s, ''
  end
end

-- lovr will run this function in its own coroutine
return function()
  local errored = false -- lovr.errhand may only be called once
  local function onerror(e, tb) -- wrapper for errhand to ensure it is only called once
    local function abortclean()
      return 1
    end
    if not errored then
      errored = true
      return lovr.errhand(e, tb) or abortclean
    else
      print('Error occurred while trying to display another error:\n' ..
        tostring(e) .. formatTraceback(tb or debug.traceback('', 2)))
      return abortclean
    end
  end

  -- Executes lovr.boot and lovr.run.
  -- continuation, afterward, will be either lovr.run's per-frame function, or the result of errhand.
  local _, continuation = xpcall(lovr.boot, onerror)

  while true do
    if type(continuation) == 'string' then -- LuaJIT returns a fixed string if an error occurs in an xpcall error handler
      print('Error occurred while trying to display another error: ' .. continuation)
      return 1
    end

    local ok, result, extra = xpcall(continuation, onerror)
    if result and ok then return result, extra -- Result is value returned by function. Return it.
    elseif not ok then continuation = result end -- Result is value returned by error handler. Make it the new error handler.

    local externerror = coroutine.yield() -- Return control to C code

    if externerror then -- A must-report error occurred in the C code
      local errorpart, tracepart = splitOnLabelLine(externerror, 'stack traceback:')
      continuation = onerror(errorpart, tracepart) -- Switch continuation to lovr.errhand
    end
  end
end
