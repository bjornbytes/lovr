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
    if not lovr.graphics then
      print(string.format('LÖVR %d.%d.%d\nNo game', lovr.getVersion()))
      lovr.event.quit()
      return
    end

    lovr.graphics.setBackgroundColor(0x20232c)
    lovr.graphics.setCullingEnabled(true)

    logo = lovr.graphics.newShader([[
      vec4 position(mat4 projection, mat4 transform, vec4 vertex) {
        return projection * transform * vertex;
      }
    ]], [[
      vec4 color(vec4 graphicsColor, sampler2D image, vec2 uv) {
        float y = (1. - uv.y);
        uv = uv * 4. - 2.;
        const float k = sqrt(3.);
        uv.x = abs(uv.x) - 1.;
        uv.y = uv.y + 1. / k + .25;
        if (uv.x + k * uv.y > 0.) {
          uv = vec2(uv.x - k * uv.y, -k * uv.x - uv.y) / 2.;
        }
        uv.x -= clamp(uv.x, -2., 0.);
        float sdf = -length(uv) * sign(uv.y) - .5;
        float w = fwidth(sdf) * .5;
        float alpha = smoothstep(.22 + w, .22 - w, sdf);
        vec3 color = mix(vec3(.094, .662, .890), vec3(.913, .275, .6), clamp(y * 1.5 - .25, 0., 1.));
        color = mix(color, vec3(.2, .2, .24), smoothstep(-.12 + w, -.12 - w, sdf));
        return vec4(pow(color, vec3(2.2)), alpha);
      }
    ]], { flags = { highp = true } })

    text = lovr.graphics.newShader('font', { flags = { highp = true } })
  end

  function lovr.draw()
    lovr.graphics.setColor(0xffffff)

    local padding = .1
    local font = lovr.graphics.getFont()
    local fade = .315 + .685 * math.abs(math.sin(lovr.timer.getTime() * 2))
    local titlePosition = 1.4 - padding
    local subtitlePosition = titlePosition - font:getHeight() * .25 - padding

    lovr.graphics.setShader(logo)
    lovr.graphics.plane('fill', 0, 1.9, -3, 1, 1, 0, 0, 1)

    lovr.graphics.setShader(text)
    lovr.graphics.setColor(0xffffff)
    lovr.graphics.print('LÖVR', -.012, titlePosition, -3, .25, 0, 0, 1, 0, nil, 'center', 'top')

    lovr.graphics.setColor(.9, .9, .9, fade)
    lovr.graphics.print('No game :(', -.005, subtitlePosition, -3, .15, 0, 0, 1, 0, nil, 'center', 'top')
    lovr.graphics.setColor(0xffffff)
    lovr.graphics.setShader()

    if lovr.headset then
      for i, hand in ipairs(lovr.headset.getHands()) do
        models[hand] = models[hand] or lovr.headset.newModel(hand, { animated = true })
        if models[hand] then
          lovr.headset.animate(hand, models[hand])

          local pose = mat4(lovr.headset.getPose(hand))
          if lovr.headset.getDriver() == 'vrapi' then
            animated = animated or lovr.graphics.newShader('unlit', { flags = { animated = true } })
            lovr.graphics.setShader(animated)
            lovr.graphics.setColorMask()
            models[hand]:draw(pose)
            lovr.graphics.setColorMask(true, true, true, true)
            lovr.graphics.setColor(0, 0, 0, .5)
            models[hand]:draw(pose)
            lovr.graphics.setShader()
          else
            models[hand]:draw(pose)
          end
        end
      end
    end

    lovr.graphics.setColor(0xffffff)
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
      debug = false
    },
    headset = {
      drivers = { 'openxr', 'oculus', 'vrapi', 'pico', 'openvr', 'webxr', 'desktop' },
      supersample = false,
      offset = 1.7,
      msaa = 4,
      overlay = false
    },
    math = {
      globals = true
    },
    window = {
      width = 1080,
      height = 600,
      fullscreen = false,
      resizable = false,
      msaa = 0,
      title = 'LÖVR',
      icon = nil,
      vsync = 1
    }
  }

  lovr.filesystem = require('lovr.filesystem')
  local hasConf, hasMain = lovr.filesystem.isFile('conf.lua'), lovr.filesystem.isFile('main.lua')
  if not lovr.filesystem.getSource() or not (hasConf or hasMain) then nogame() end

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

  if lovr.headset and lovr.graphics and conf.window then
    lovr.headset.start()
  end

  lovr.handlers = setmetatable({}, { __index = lovr })
  if not confOk then error(confError) end
  if hasMain then require 'main' end
  return lovr.run()
end

function lovr.run()
  local dt = 0
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
    if lovr.timer then dt = lovr.timer.step() end
    if lovr.headset then lovr.headset.update(dt) end
    if lovr.update then lovr.update(dt) end
    if lovr.graphics and lovr.draw then
      lovr.graphics.begin()
      lovr.graphics.renderTo('window', lovr.draw)
      lovr.graphics.submit()
    end
    if lovr.math then lovr.math.drain() end
  end
end

function lovr.mirror()
  --
end

local function formatTraceback(s)
  return s:gsub('\n[^\n]+$', ''):gsub('\t', ''):gsub('stack traceback', '\nStack')
end

function lovr.errhand(message, traceback)
  message = tostring(message)
  message = message .. formatTraceback(traceback or debug.traceback('', 4))
  print('Error:\n' .. message)
  if not lovr.graphics then return function() return 1 end end

  lovr.graphics.reset()
  lovr.graphics.setBackgroundColor(.11, .10, .14)
  lovr.graphics.setColor(.85, .85, .85)
  local font = lovr.graphics.getFont()
  font:setPixelDensity()
  font:setFlipEnabled(false)
  local wrap = .7 * font:getPixelDensity()
  local width, lines = font:getWidth(message, wrap)
  local height = 2.6 + lines
  local y = math.min(height / 2, 10)
  local function render()
    lovr.graphics.print('Error', -width / 2, y, -20, 1.6, 0, 0, 0, 0, nil, 'left', 'top')
    lovr.graphics.print(message, -width / 2, y - 2.6, -20, 1.0, 0, 0, 0, 0, wrap, 'left', 'top')
  end

  return function()
    lovr.event.pump()
    for name, a in lovr.event.poll() do
      if name == 'quit' then return a or 1
      elseif name == 'restart' then return 'restart', lovr.restart and lovr.restart() end
    end
    lovr.graphics.origin()
    if lovr.headset then
      lovr.headset.update(0)
      lovr.headset.renderTo(render)
    end
    if lovr.graphics.hasWindow() then
      lovr.graphics.setViewPose(1)
      local width, height = lovr.graphics.getDimensions()
      local projection = lovr.math.mat4():perspective(.1, 100, math.rad(67), width / height)
      lovr.graphics.setProjection(1, projection)
      lovr.graphics.clear()
      render()
    end
    lovr.graphics.present()
    if lovr.math then
      lovr.math.drain()
    end
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
