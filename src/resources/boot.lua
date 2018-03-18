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
    drivers = { 'openvr', 'webvr', 'fake' },
    mirror = true,
    offset = 1.7
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

function lovr.errhand(message)
  message = 'Error:\n' .. message:gsub('\n[^\n]+$', ''):gsub('\t', ''):gsub('stack traceback', '\nStack')
  print(message)
  if not lovr.graphics then return end
  lovr.graphics.reset()
  lovr.graphics.setBackgroundColor(.105, .098, .137)
  lovr.graphics.setColor(.863, .863, .863)
  if lovr.headset then
    lovr.headset.setMirrored(false)
  end
  local font = lovr.graphics.getFont()
  local pixelDensity = font:getPixelDensity()
  local width = font:getWidth(message, .55 * pixelDensity)
  local function render()
    lovr.graphics.print(message, -width / 2, 0, -20, 1, 0, 0, 0, 0, .55 * pixelDensity, 'left')
  end
  while true do
    lovr.event.pump()
    for name in lovr.event.poll() do
      if name == 'quit' then return end
    end
    lovr.graphics.clear()
    lovr.graphics.origin()
    if lovr.headset and lovr.getOS() ~= 'Web' then
      lovr.graphics.push()
      lovr.graphics.translate(0, conf.headset.offset, 0)
      lovr.headset.renderTo(render)
      lovr.graphics.pop()
    end
    render()
    lovr.graphics.present()
    lovr.timer.sleep(lovr.headset and .001 or .1)
  end
end

lovr.filesystem = require('lovr.filesystem')

local runnable = lovr.filesystem.isFile('conf.lua') or lovr.filesystem.isFile('main.lua')
if not lovr.filesystem.getSource() or not runnable then
  function lovr.conf(t)
    t.modules.audio = false
    t.modules.math = false
    t.modules.physics = false
  end

  local logo, controllers

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

    lovr.graphics.plane(logo, 0, 1.9, -3, 1, 0, 0, 1)
    lovr.graphics.setColor(.059, .059, .059)
    lovr.graphics.print('LÖVR', -.01, titlePosition, -3, .25, 0, 0, 1, 0, nil, 'center', 'top')
    lovr.graphics.setColor(.059, .059, .059, fade)
    lovr.graphics.print('No game :(', -.01, subtitlePosition, -3, .15, 0, 0, 1, 0, nil, 'center', 'top')
  end

  function refreshControllers()
    controllers = {}

    if not lovr.headset then return end

    for _, controller in pairs(lovr.headset.getControllers()) do
      controllers[controller] = controller:newModel()
    end
  end

  lovr.controlleradded = refreshControllers
  lovr.controllerremoved = refreshControllers
end

local confOk, confError
if lovr.filesystem.isFile('conf.lua') then
  confOk, confError = pcall(require, 'conf')
  if lovr.conf then
    confOk, confError = pcall(lovr.conf, conf)
  end
end

lovr._setConf(conf)

lovr.filesystem.setIdentity(conf.identity)

local modules = { 'audio', 'data', 'event', 'graphics', 'headset', 'math', 'physics', 'thread', 'timer' }
for _, module in ipairs(modules) do
  if conf.modules[module] then
    lovr[module] = require('lovr.' .. module)
  end
end

-- Error after window is created
if confError then
  error(confError)
end

lovr.handlers = setmetatable({}, { __index = lovr })

function lovr.step()
  lovr.event.pump()
  for name, a, b, c, d in lovr.event.poll() do
    if name == 'quit' and (not lovr.quit or not lovr.quit()) then
      return a
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
    lovr.graphics.clear()
    lovr.graphics.origin()
    if lovr.draw then
      if lovr.headset then
        lovr.headset.renderTo(lovr.draw)
      else
        lovr.draw()
      end
    end
    lovr.graphics.present()
  end
  lovr.timer.sleep(.001)
end

function lovr.run()
  lovr.timer.step()
  if lovr.load then lovr.load() end
  while true do
    local exit = lovr.step()
    if exit then return exit end
  end
end

function lovr.threaderror(thread, err)
  error('Thread error\n\n' .. err)
end

if lovr.filesystem.isFile('main.lua') then
  require('main')
end
