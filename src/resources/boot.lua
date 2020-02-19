lovr = require 'lovr'

local function nogame()
  function lovr.conf(t)
    t.modules.audio = false
    t.modules.math = false
    t.modules.physics = false
    t.modules.thread = false
  end

  local shader
  local models = {}

  function lovr.load()
    if not lovr.graphics then
      print(string.format('LÖVR %d.%d.%d\nNo game', lovr.getVersion()))
      lovr.event.quit()
      return
    end

    lovr.graphics.setBackgroundColor(.894, .933, .949)

    shader = lovr.graphics.newShader([[
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
        float w = fwidth(sdf);
        float alpha = smoothstep(.22 + w, .22 - w, sdf);
        vec3 color = mix(vec3(.094, .662, .890), vec3(.913, .275, .6), clamp(y * 1.5 - .25, 0., 1.));
        color = mix(color, vec3(1.), smoothstep(-.12 + w, -.12 - w, sdf));
        return vec4(pow(color, vec3(2.2)), alpha);
      }
    ]])
  end

  function lovr.draw()
    lovr.graphics.setColor(0xffffff)

    if lovr.headset then
      for i, hand in ipairs(lovr.headset.getHands()) do
        models[hand] = models[hand] or lovr.headset.newModel(hand)
        if models[hand] then
          local x, y, z, angle ax, ay, az = lovr.headset.getPose(hand)
          models[hand]:draw(x, y, z, 1.0, angle, ax, ay, az)
        end
      end
    end

    local padding = .1
    local font = lovr.graphics.getFont()
    local fade = .315 + .685 * math.abs(math.sin(lovr.timer.getTime() * 2))
    local titlePosition = 1.4 - padding
    local subtitlePosition = titlePosition - font:getHeight() * .25 - padding

    lovr.graphics.setShader(shader)
    lovr.graphics.plane('fill', 0, 1.9, -3, 1, 1, 0, 0, 1)
    lovr.graphics.setShader()

    lovr.graphics.setColor(.059, .059, .059)
    lovr.graphics.print('LÖVR', -.012, titlePosition, -3, .25, 0, 0, 1, 0, nil, 'center', 'top')

    lovr.graphics.setColor(.059, .059, .059, fade)
    lovr.graphics.print('No game :(', -.005, subtitlePosition, -3, .15, 0, 0, 1, 0, nil, 'center', 'top')

    lovr.graphics.setColor(0xffffff)
  end
end

-- Note: Cannot be overloaded
function lovr.boot()
  local conf = {
    version = '0.13.0',
    identity = 'default',
    hotkeys = true,
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
    headset = {
      drivers = { 'leap', 'openxr', 'oculus', 'oculusmobile', 'openvr', 'webvr', 'desktop' },
      offset = 1.7,
      msaa = 4
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
    if lovr.math then
      lovr.math.drain()
    end
  end
end

function lovr.mirror()
  if lovr.headset then -- On some systems, headset module will be disabled
    local texture = lovr.headset.getMirrorTexture()
    if texture then    -- On some drivers, texture is printed directly to the window
      if lovr.headset.getDriver() == 'oculus' then
        lovr.graphics.fill(lovr.headset.getMirrorTexture(), 0, 1, 1, -1)
      else
        lovr.graphics.fill(lovr.headset.getMirrorTexture())
      end
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
  message = message .. formatTraceback(traceback or debug.traceback('', 4))
  print('Error:\n' .. message)
  if not lovr.graphics then return function() return 1 end end

  lovr.graphics.reset()
  lovr.graphics.setBackgroundColor(.11, .10, .14)
  lovr.graphics.setColor(.85, .85, .85)
  local font = lovr.graphics.getFont()
  font:setFlipEnabled(false)
  local wrap = .7 * font:getPixelDensity()
  local width, lines = font:getWidth(message, wrap)
  local height = 2.6 + lines
  local y = math.min(height / 2, 10)
  local function render(window)
    lovr.graphics.print('Error', -width / 2, y, -20, 1.6, 0, 0, 0, 0, nil, 'left', 'top')
    lovr.graphics.print(message, -width / 2, y - 2.6, -20, 1.0, 0, 0, 0, 0, wrap, 'left', 'top')
  end

  return function()
    lovr.event.pump()
    for name, a in lovr.event.poll() do if name == 'quit' then return a or 1 end end
    lovr.graphics.origin()
    if lovr.headset then
      lovr.headset.update(0)
      lovr.headset.renderTo(render)
    end
    if lovr.graphics.hasWindow() then
      lovr.graphics.clear()
      render(true)
    end
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
      print('Error occurred while trying to display another error: ' .. continuation)
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
