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

    lovr.graphics.setBackground(0x20232c)

    logo = lovr.graphics.newShader([[
      vec4 lovrmain() {
        return DefaultPosition;
      }
    ]], [[
      vec4 lovrmain() {
        float y = FragUV.y;
        vec2 uv = vec2(FragUV.x, 1. - FragUV.y);
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
    ]])
  end

  function lovr.draw(pass)
    local padding = .1
    local font = lovr.graphics.getDefaultFont()
    local fade = .315 + .685 * math.abs(math.sin(lovr.headset.getTime() * 2))
    local titlePosition = 1.5 - padding
    local subtitlePosition = titlePosition - font:getHeight() * .25 - padding

    pass:setCullMode('back')
    pass:setBlendMode('alpha')
    pass:setShader(logo)
    pass:plane(0, 2, -3)

    pass:setShader()
    pass:setColor(1, 1, 1)
    pass:text('LÖVR', -.012, titlePosition, -3, .25, quat(0, 0, 1, 0), nil, 'center', 'top')

    pass:setColor(.9, .9, .9, fade)
    pass:text('No game :(', -.005, subtitlePosition, -3, .15, 0, 0, 1, 0, nil, 'center', 'top')
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
  local main = arg[0] and arg[0]:match('[^\\/]-%.lua$') or 'main.lua'
  local hasConf, hasMain = lovr.filesystem.isFile('conf.lua'), lovr.filesystem.isFile(main)
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
      local headset = lovr.headset and lovr.headset.getTexture()
      if headset then
        local pass = lovr.graphics.getPass('render', headset)
        local near, far = lovr.headset.getClipDistance()
        for i = 1, lovr.headset.getViewCount() do
          pass:setViewPose(i, lovr.headset.getViewPose(i))
          local left, right, up, down = lovr.headset.getViewAngles(i)
          pass:setProjection(i, left, right, up, down, near, far)
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
  if not lovr.graphics then return function() return 1 end end

  local function render(pass)
    pass:setBlendMode('alpha')
    pass:text('Error', 0, .3, -1, .1)
    pass:text(message, 0, 0, -1, .05)
  end

  lovr.graphics.submit()
  lovr.graphics.setBackground(.11, .10, .14)
  return function()
    lovr.event.pump()
    for name, a in lovr.event.poll() do
      if name == 'quit' then return a or 1
      elseif name == 'restart' then return 'restart', lovr.event.restart and lovr.restart() end
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
      local projection = lovr.math.mat4():perspective(0.1, 100.0, 1.0, width / height)
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
