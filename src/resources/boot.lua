local function nogame()
  local logo, controllers
  local function refreshControllers()
    controllers = {}
    if not lovr.headset then return end
    for _, controller in pairs(lovr.headset.getControllers()) do
      controllers[controller] = controller:newModel()
    end
  end

  function lovr.conf(t)
    t.modules.audio = false
    t.modules.math = false
    t.modules.physics = false
    t.modules.thread = false
  end

  function lovr.load()
    local texture = lovr.graphics.newTexture(lovr.data.newBlob(lovr._logo, 'logo.png'))
    logo = lovr.graphics.newMaterial(texture)
    lovr.graphics.setBackgroundColor(.960, .988, 1.0)
    refreshControllers()
  end

  function lovr.draw()
    lovr.graphics.setColor(1.0, 1.0, 1.0)
    for controller, model in pairs(controllers) do
      local x, y, z = controller:getPosition()
      model:draw(x, y, z, 1, controller:getOrientation())
    end
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
  end

  lovr.controlleradded = refreshControllers
  lovr.controllerremoved = refreshControllers
end

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
    graphics = {
      singlepass = true,
    },
    headset = {
      drivers = { 'openvr', 'webvr', 'fake' },
      mirror = true,
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

  local ok, confError
  if hasConf then ok, confError = pcall(require, 'conf') end
  if lovr.conf then ok, confError = pcall(lovr.conf, conf) end

  lovr._setConf(conf)
  lovr.filesystem.setIdentity(conf.identity)

  local modules = { 'audio', 'data', 'event', 'graphics', 'headset', 'math', 'physics', 'thread', 'timer' }
  for _, module in ipairs(modules) do
    if conf.modules[module] then
      lovr[module] = require('lovr.' .. module)
    end
  end

  lovr.handlers = setmetatable({}, { __index = lovr })
  if confError then error(confError) end
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
        lovr.audio.setOrientation(lovr.headset.getOrientation())
        lovr.audio.setPosition(lovr.headset.getPosition())
        lovr.audio.setVelocity(lovr.headset.getVelocity())
      end
    end
    if lovr.update then lovr.update(dt) end
    if lovr.graphics then
      lovr.graphics.origin()
      if lovr.draw then
        if lovr.headset then
          lovr.headset.renderTo(lovr.draw)
        else
          lovr.graphics.clear()
          lovr.draw()
        end
      end
      lovr.graphics.present()
    end
  end
end

function lovr.errhand(message)
  message = debug.traceback('Error:\n' .. message, 2):gsub('\n[^\n]+$', ''):gsub('\t', ''):gsub('stack traceback', '\nStack')
  print(message)
  if not lovr.graphics then return function() return 1 end end
  lovr.graphics.reset()
  lovr.graphics.setBackgroundColor(.105, .098, .137)
  lovr.graphics.setColor(.863, .863, .863)
  if lovr.headset then lovr.headset.setMirrored(false) end
  local font = lovr.graphics.getFont()
  local pixelDensity = font:getPixelDensity()
  local width = font:getWidth(message, .55 * pixelDensity)
  local function render()
    lovr.graphics.print(message, -width / 2, 0, -20, 1, 0, 0, 0, 0, .55 * pixelDensity, 'left')
  end
  return function()
    lovr.event.pump()
    for name in lovr.event.poll() do if name == 'quit' then return 1 end end
    lovr.graphics.clear()
    lovr.graphics.origin()
    if lovr.headset then lovr.headset.renderTo(render) end
    render()
    lovr.graphics.present()
  end
end

function lovr.threaderror(thread, err)
  error('Thread error\n\n' .. err, 0)
end

return function()
  local errored = false
  local function onerror(...) if not errored then errored = true return lovr.errhand(...) else return function() return 1 end end end
  local _, thread = xpcall(lovr.boot, onerror)

  while true do
    local ok, result = xpcall(thread, onerror)
    if result and ok then return result
    elseif not ok then thread = result end
    coroutine.yield()
  end

  return 1
end
