lovr = require 'lovr'

local function nogame()
  function lovr.conf(t)
    t.headset.supersample = true
    t.modules.audio = false
    t.modules.physics = false
    t.modules.thread = false
  end

  local models = {}

  function lovr.load()
    print(string.format('LÖVR %d.%d.%d\nNo game', lovr.getVersion()))
    lovr.event.quit()
  end

  function lovr.draw()
    --
  end
end

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
      vsync = false
    },
    headset = {
      drivers = { 'openxr', 'webxr', 'desktop' },
      supersample = false,
      offset = 1.7,
      msaa = 4,
      overlay = false
    },
    math = {
      globals = true
    },
    window = {
      width = 800,
      height = 600,
      fullscreen = false,
      resizable = false,
      title = 'LÖVR',
      icon = nil
    }
  }

  lovr.filesystem = require('lovr.filesystem')
  local hasConf, hasMain = lovr.filesystem.isFile('conf.lua'), lovr.filesystem.isFile('main.lua')
  if not lovr.filesystem.getSource() or not (hasConf or hasMain) then nogame() end

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
  if hasMain then require 'main' end
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
      local headset = lovr.headset and lovr.headset.getTexture()
      if headset then
        local pass = lovr.graphics.getPass('render', headset)
        for i = 1, lovr.headset.getViewCount() do
          pass:setViewPose(i, lovr.headset.getViewPose(i))
          pass:setProjection(i, lovr.headset.getViewAngles(i))
        end
        local skip = lovr.draw and lovr.draw(pass)
        if not skip then lovr.graphics.submit(pass) end
      end
      if lovr.system.isWindowOpen() then
        lovr.mirror(pass)
      end
    end
    if lovr.headset then lovr.headset.submit() end
    if lovr.math then lovr.math.drain() end
  end
end

function lovr.mirror(pass)
  if lovr.headset then
    --
  else
    local pass = lovr.graphics.getPass('render', 'window')
    local skip = lovr.draw and lovr.draw(pass)
    if not skip then lovr.graphics.submit(pass) end
  end
end

local function formatTraceback(s)
  return s:gsub('\n[^\n]+$', ''):gsub('\t', ''):gsub('stack traceback', '\nStack')
end

function lovr.errhand(message, traceback)
  message = tostring(message)
  message = message .. formatTraceback(traceback or debug.traceback('', 4))
  print('Error:\n' .. message)
  return function() return 1 end
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
