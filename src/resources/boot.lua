lovr = require 'lovr'

local function nogame()
  function lovr.conf(t)
    t.modules.audio = false
    t.modules.math = false
    t.modules.physics = false
    t.modules.thread = false
  end

  function lovr.load()
    if not lovr.graphics then
      print(string.format('LÖVR %d.%d.%d\nNo game', lovr.getVersion()))
      lovr.event.quit()
      return
    end
    local texture = lovr.graphics.newTexture(lovr.data.newBlob(lovr._logo, 'logo.png'))
    logo = lovr.graphics.newMaterial(texture)
    lovr.graphics.setBackgroundColor(.960, .988, 1.0)
  end

  function lovr.draw()
    lovr.graphics.setColor(1.0, 1.0, 1.0)
    local padding = .1
    local font = lovr.graphics.getFont()
    local fade = .315 + .685 * math.abs(math.sin(lovr.timer.getTime() * 2))
    local titlePosition = 1.4 - padding
    local subtitlePosition = titlePosition - font:getHeight() * .25 - padding
    lovr.graphics.plane(logo, 0, 1.9, -3, 1, 1, 0, 0, 1)
    lovr.graphics.setColor(.059, .059, .059)
    lovr.graphics.print('LÖVR', -.01, titlePosition, -3, .25, 0, 0, 1, 0, nil, 'center', 'top')
    lovr.graphics.setColor(.059, .059, .059, fade)
    lovr.graphics.print('No game :(', -.01, subtitlePosition, -3, .15, 0, 0, 1, 0, nil, 'center', 'top')
    lovr.graphics.setColor(1, 1, 1)
  end
end

-- Note: Cannot be overloaded
function lovr.boot()
  local conf = {
    modules = {
      audio = true,
      data = true,
      event = true,
      graphics = true,
      headset = true,
      math = true,
      physics = true,
      thread = true,
      timer = true
    },
    gammacorrect = false,
    headset = {
      drivers = { 'oculus', 'oculusmobile', 'openvr', 'webvr', 'desktop' },
      offset = 1.7,
      msaa = 4
    },
    window = {
      width = 1080,
      height = 600,
      fullscreen = false,
      msaa = 0,
      title = 'LÖVR',
      icon = nil
    }
  }

  lovr.filesystem = require('lovr.filesystem')
  local hasConf, hasMain = lovr.filesystem.isFile('conf.lua'), lovr.filesystem.isFile('main.lua')
  if not lovr.filesystem.getSource() or not (hasConf or hasMain) then nogame() end

  local confOk, confError = true
  if hasConf then confOk, confError = pcall(require, 'conf') end
  if confOk and lovr.conf then confOk, confError = pcall(lovr.conf, conf) end

  lovr._setConf(conf)
  lovr.filesystem.setIdentity(conf.identity)

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

  lovr.handlers = setmetatable({}, { __index = lovr })
  if not confOk then error(confError) end
  if hasMain then require 'main' end
  return lovr.run()
end

function lovr.run()
  lovr.timer.step()
  if lovr.load then lovr.load(arg) end
  return function()
    lovr.event.pump()
    for name, a, b, c, d in lovr.event.poll() do
      if name == 'quit' and (not lovr.quit or not lovr.quit()) then
        return a or 0
      end
      if lovr.handlers[name] then lovr.handlers[name](a, b, c, d) end
    end
    local dt = lovr.timer.step()
    if lovr.headset then
      lovr.headset.update(dt)
    end
    if lovr.audio then
      lovr.audio.update()
      if lovr.headset then
        lovr.audio.setPose(lovr.headset.getPose())
        lovr.audio.setVelocity(lovr.headset.getVelocity())
      end
    end
    if lovr.update then lovr.update(dt) end
    if lovr.graphics then
      lovr.graphics.origin()
      if lovr.draw then
        if lovr.headset then
          lovr.headset.renderTo(lovr.draw)
        end
        if lovr.graphics.hasWindow() then
          lovr.mirror()
        end
      end
      lovr.graphics.present()
    end
  end
end

function lovr.mirror()
  if lovr.headset then -- On some systems, headset module will be disabled
    local texture = lovr.headset.getMirrorTexture()
    if texture then    -- On some drivers, texture is printed directly to the window
      lovr.graphics.fill(texture)
    end
  else
    lovr.graphics.clear()
    lovr.draw()
  end
end

local function formatTraceback(s)
  return s:gsub('\n[^\n]+$', ''):gsub('\t', ''):gsub('stack traceback', '\nStack')
end

function lovr.errhand(message, traceback)
  message = tostring(message)
  message = 'Error:\n' .. message .. formatTraceback(traceback or debug.traceback('', 4))
  print(message)
  if not lovr.graphics then return function() return 1 end end
  lovr.graphics.reset()
  lovr.graphics.setBackgroundColor(.105, .098, .137)
  lovr.graphics.setColor(.863, .863, .863)
  local font = lovr.graphics.getFont()
  font:setFlipEnabled(false)
  local pixelDensity = font:getPixelDensity()
  local width = font:getWidth(message, .55 * pixelDensity)
  local function render()
    lovr.graphics.print(message, -width / 2, 0, -20, 1, 0, 0, 0, 0, .55 * pixelDensity, 'left')
  end
  return function()
    lovr.event.pump()
    for name in lovr.event.poll() do if name == 'quit' then return 1 end end
    lovr.headset.update(0)
    lovr.graphics.origin()
    if lovr.headset then lovr.headset.renderTo(render) end
    lovr.graphics.clear()
    render()
    lovr.graphics.present()
  end
end

function lovr.threaderror(thread, err)
  error('Thread error\n\n' .. err, 0)
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
      print('Error occurred while trying to display another error.')
      return 1
    end

    local ok, result = xpcall(continuation, onerror)
    if result and ok then return result -- Result is value returned by function. Return it.
    elseif not ok then continuation = result end -- Result is value returned by error handler. Make it the new error handler.

    local externerror = coroutine.yield() -- Return control to C code

    if externerror then -- A must-report error occurred in the C code
      local errorpart, tracepart = splitOnLabelLine(externerror, 'stack traceback:')
      continuation = onerror(errorpart, tracepart) -- Switch continuation to lovr.errhand
    end
  end
end
