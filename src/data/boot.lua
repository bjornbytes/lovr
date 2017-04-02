local conf = {
  modules = {
    audio = true,
    event = true,
    graphics = true,
    headset = true,
    math = true,
    timer = true
  },
  headset = {
    mirror = true
  }
}

lovr.filesystem = require('lovr.filesystem')

local runnable = lovr.filesystem.isFile('conf.lua') or lovr.filesystem.isFile('main.lua')
if not lovr.filesystem.getSource() or not runnable then
  function lovr.conf(t)
    t.modules.audio = false
    t.modules.math = false
  end

  function lovr.load()
    lovr.filesystem.write('asdf.png', lovr._logo)
    logo = lovr.graphics.newTexture(lovr.filesystem.newBlob(lovr._logo, 'logo.png'))
    lovr.graphics.setBackgroundColor(245, 252, 255)
    refreshControllers()
  end

  function lovr.draw()
    if lovr.headset and lovr.headset.isPresent() then
      ground = ground or lovr.graphics.newMesh(lovr.headset.getBoundsGeometry())

      lovr.graphics.setColor(255, 255, 255)
      ground:draw()

      lovr.graphics.setColor(255, 255, 255)
      for controller, model in pairs(controllers) do
        local x, y, z = controller:getPosition()
        model:draw(x, y, z, 1, controller:getOrientation())
      end

      lovr.graphics.translate(0, 2, 0)
    else
      lovr.graphics.translate(0, .2, 0)
    end

    local padding = .1
    local font = lovr.graphics.getFont()
    local fade = 80 + 150 * math.abs(math.sin(lovr.timer.getTime() * 2))
    local titlePosition = -.5 - padding
    local subtitlePosition = titlePosition - font:getHeight() * .25 - padding

    lovr.graphics.setColor(255, 255, 255)
    lovr.graphics.plane(logo, 0, 0, -3, 1, 0, 0, 1)
    lovr.graphics.setColor(15, 15, 15)
    lovr.graphics.print('LÖVR', -.01, titlePosition, -3, .25, 0, 0, 1, 0, nil, 'center', 'top')
    lovr.graphics.setColor(15, 15, 15, fade)
    lovr.graphics.print('No game :(', -.01, subtitlePosition, -3, .15, 0, 0, 1, 0, nil, 'center', 'top')
  end

  function refreshControllers()
    if not lovr.headset then return end

    controllers = {}

    for _, controller in pairs(lovr.headset.getControllers()) do
      controllers[controller] = controller:newModel()
    end
  end

  lovr.controlleradded = refreshControllers
  lovr.controllerremoved = refreshControllers
end

local success, err = pcall(require, 'conf')
if lovr.conf then
  success, err = pcall(lovr.conf, conf)
end

local modules = { 'audio', 'event', 'graphics', 'headset', 'math', 'timer' }
for _, module in ipairs(modules) do
  if conf.modules[module] then
    lovr[module] = require('lovr.' .. module)
  end
end

lovr.filesystem.setIdentity(conf.identity or 'default')
if lovr.headset then lovr.headset.setMirrored(conf.headset and conf.headset.mirror) end

lovr.handlers = setmetatable({
  quit = function() end,
  focus = function(f)
    if lovr.focus then lovr.focus(f) end
  end,
  controlleradded = function(c)
    if lovr.controlleradded then lovr.controlleradded(c) end
  end,
  controllerremoved = function(c)
    if lovr.controllerremoved then lovr.controllerremoved(c) end
  end,
  controllerpressed = function(c, b)
    if lovr.controllerpressed then lovr.controllerpressed(c, b) end
  end,
  controllerreleased = function(c, b)
    if lovr.controllerreleased then lovr.controllerreleased(c, b) end
  end
}, {
  __index = function(self, event)
    error('Unknown event: ' .. tostring(event))
  end
})

function lovr.run()
  if lovr.load then lovr.load() end
  while true do
    lovr.event.pump()
    for name, a, b, c, d in lovr.event.poll() do
      if name == 'quit' and (not lovr.quit or not lovr.quit()) then
        return a
      end
      lovr.handlers[name](a, b, c, d)
    end
    local dt = lovr.timer.step()
    if lovr.audio then
      lovr.audio.update()
      if lovr.headset and lovr.headset.isPresent() then
        lovr.audio.setOrientation(lovr.headset.getOrientation())
        lovr.audio.setPosition(lovr.headset.getPosition())
        lovr.audio.setVelocity(lovr.headset.getVelocity())
      end
    end
    if lovr.update then lovr.update(dt) end
    lovr.graphics.clear()
    lovr.graphics.origin()
    if lovr.draw then
      if lovr.headset and lovr.headset.isPresent() then
        lovr.headset.renderTo(lovr.draw)
      else
        lovr.draw()
      end
    end
    lovr.graphics.present()
    lovr.timer.sleep(.001)
  end
end

function lovr.errhand(message)
  message = 'Error:\n' .. message:gsub('\n[^\n]+$', ''):gsub('\t', ''):gsub('stack traceback', '\nStack')
  print(message)
  lovr.graphics.reset()
  lovr.graphics.setBackgroundColor(27, 25, 35)
  lovr.graphics.setColor(220, 220, 220)
  local font = lovr.graphics.getFont()
  local pixelDensity = font:getPixelDensity()
  local width = font:getWidth(message, .55 * pixelDensity)
  local function render()
    lovr.graphics.origin()
    lovr.graphics.print(message, -width / 2, 0, -20, 1, 0, 0, 0, 0, .55 * pixelDensity, 'left')
  end
  while true do
    lovr.event.pump()
    for name in lovr.event.poll() do
      if name == 'quit' then return end
    end
    lovr.graphics.clear()
    if lovr.headset and lovr.headset.isPresent() then
      lovr.headset.renderTo(render)
    else
      render()
    end
    lovr.graphics.present()
    lovr.timer.sleep((lovr.headset and lovr.headset.isPresent()) and .001 or .1)
  end
end

if lovr.filesystem.isFile('main.lua') then
  require('main')
end
