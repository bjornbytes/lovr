function lovr.errhand(message)
  local traceback = (debug and debug.traceback('', 4) or ''):gsub('\n[^\n]+$', ''):gsub('\t', ''):gsub('stack traceback:', '\nStack:\n')

  print('Error:\n\n' .. tostring(message) .. traceback)

  local hasHeadset = lovr.headset and (lovr.headset.isActive() or lovr.system.isWindowOpen())

  if not lovr.graphics or not lovr.graphics.isInitialized() or not hasHeadset then
    return function() return 1 end
  end

  local headerSize = 32
  local textSize = 24
  local margin = 16
  local padding = 24
  local border = 3
  local borderRadius = 8

  local stack = {}
  local level
  local frame

  local layout = {}
  local panel = 1
  local stackColumnWidth = 0
  local sourceScroll = 0
  local dirty = true

  local thumbsticks = {}

  local ok, font = pcall(lovr.graphics.newFont, 'ZhiMaMono-Regular.ttf')

  if not ok then
    font = lovr.graphics.getDefaultFont()
  end

  local function clamp(x, min, max)
    return math.min(math.max(x, min), max)
  end

  local function inside(x, y, rect)
    return x >= rect.x and x <= rect.x + rect.w and y >= rect.y and y <= rect.y + rect.h
  end

  local function scrollSource(value)
    local limit = math.max(#stack[level].lines * textSize - (layout.source.h - 2 * padding), 0)
    sourceScroll = clamp(value, 0, limit)
  end

  local function setLevel(index)
    local prev = level

    level = clamp(index, 1, #stack)
    frame = stack[level]

    if level ~= prev and stack[level].lines and stack[level].currentline ~= -1 then
      scrollSource((frame.currentline + .5) * textSize - (layout.source.h - padding) / 2)
    end
  end

  local function buildVariableTable(frame)
    frame.rows = {}
    frame.columnWidth = 0

    local function halp(name, value, parent, id, depth)
      frame.columnWidth = math.max(frame.columnWidth, font:getWidth(name) * textSize + depth * padding)

      local row = {
        name = name,
        value = value,
        parent = parent,
        index = #frame.rows + 1,
        depth = depth
      }

      row.contents = type(row.value) == 'string' and string.format("'%s'", row.value) or tostring(row.value)
      row.contents = row.contents:gsub('\n', '\\n')

      if #row.contents > 10000 then
        row.contents = row.contents:sub(1, 10000) .. '...'
      end

      table.insert(frame.rows, row)

      if type(value) == 'table' then
        row.id = id .. string.format('%p:%s', name, value)

        if frame.expanded[row.id] then
          if next(value) then
            for k, v in pairs(value) do
              halp(tostring(k), v, row, row.id, depth + 1)
            end
          else
            table.insert(frame.rows, {
              name = '',
              value = nil,
              contents = '<empty>',
              parent = row,
              index = #frame.rows + 1,
              depth = depth + 1
            })
          end
        end
      end
    end

    for i, var in ipairs(frame.variables) do
      halp(var[1], var[2], nil, '', 0)
    end
  end

  local function clampVariableScroll()
    frame.scroll = clamp(frame.scroll, 0, math.max(#frame.rows * textSize - (layout.variables.h - 2 * padding), 0))
  end

  local function ensureVariableVisible()
    frame.scroll = clamp(frame.scroll, frame.rowIndex * textSize - (layout.variables.h - 2 * padding), (frame.rowIndex - 1) * textSize)
  end

  local function resize(width, height)
    layout.message = {
      x = padding,
      y = padding + margin,
      w = width - 2 * padding,
      h = #font:getLines(message, (width - 4 * padding) / textSize) * textSize + 2 * padding
    }

    layout.stack = {
      x = padding,
      y = layout.message.y + layout.message.h + padding + margin,
      w = width - 2 * padding,
      h = textSize * #stack + 2 * padding
    }

    local rowY = layout.stack.y + layout.stack.h + padding + margin

    layout.source = {
      x = padding,
      y = rowY,
      w = width / 2 - 1.5 * padding,
      h = height - padding - rowY
    }

    layout.variables = {
      x = width / 2 + padding / 2,
      y = rowY,
      w = width / 2 - 1.5 * padding,
      h = height - padding - rowY
    }
  end

  local function up()
    if panel == 1 then
      setLevel(level - 1)
    elseif panel == 2 then
      scrollSource(sourceScroll - textSize)
    elseif panel == 3 then
      frame.rowIndex = math.max(frame.rowIndex - 1, 1)
      ensureVariableVisible()
    end
  end

  local function down()
    if panel == 1 then
      setLevel(level + 1)
    elseif panel == 2 then
      scrollSource(sourceScroll + textSize)
    elseif panel == 3 then
      frame.rowIndex = math.min(frame.rowIndex + 1, #frame.rows)
      ensureVariableVisible()
    end
  end

  local function left()
    if panel == 3 then
      local row = frame.rows[frame.rowIndex]
      if row then
        if frame.expanded[row.id] then
          frame.expanded[row.id] = false
          buildVariableTable(frame)
          ensureVariableVisible()
        elseif row.parent and frame.expanded[row.parent.id] then
          frame.rowIndex = row.parent.index
          frame.expanded[row.parent.id] = false
          buildVariableTable(frame)
          ensureVariableVisible()
        end
      end
    end
  end

  local function right()
    if panel == 3 then
      local row = frame.rows[frame.rowIndex]
      if row and type(row.value) == 'table' then
        frame.expanded[row.id] = true
        buildVariableTable(frame)
        ensureVariableVisible()
      end
    end
  end

  local function top()
    if panel == 1 then
      setLevel(1)
    elseif panel == 2 then
      scrollSource(0)
    elseif panel == 3 then
      frame.rowIndex = 1
      ensureVariableVisible()
    end
  end

  local function bottom()
    if panel == 1 then
      setLevel(#stack)
    elseif panel == 2 then
      scrollSource(math.huge)
    elseif panel == 3 then
      frame.rowIndex = #frame.rows
      ensureVariableVisible()
    end
  end

  local function focusNextPanel()
    panel = 1 + (panel % 3)
  end

  local function render(pass)
    if not pass then return end

    resize(pass:getDimensions())

    local function section(pass, title, layout, focused, callback)
      local x, y, w, h = layout.x, layout.y, layout.w, layout.h
      local titleWidth = font:getWidth(title) * headerSize

      if h <= 2 * padding then return end

      -- Border
      pass:setColor(focused and 0x6747c4 or 0x404040)
      pass:roundrect(x + w / 2, y + h / 2, 0, w + border, h + border, 0, nil, borderRadius, 4)

      -- Background
      pass:setColor(.11, .10, .14)
      pass:roundrect(x + w / 2, y + h / 2, 0, w - border, h - border, 0, nil, borderRadius, 4)
      pass:plane(x + padding + titleWidth / 2, y, 0, titleWidth + padding, headerSize)

      -- Title
      pass:setColor(focused and 0xc0c0c0 or 0x808080)
      pass:text(title, x + padding, y, 0, headerSize, nil, 0, 'left', 'middle')

      -- Contents
      pass:setScissor(x, y + padding, w, h - 2 * padding)
      callback(x, y, w, h)
      pass:setScissor()
    end

    pass:setProjection('orthographic')
    pass:setDepthTest()
    pass:setFont(font)

    section(pass, 'Error', layout.message, false, function(x, y, w, h)
      pass:setColor(0xf0f0f0)
      pass:text(message, x + padding, y + padding, 0, textSize, nil, (w - 2 * padding) / textSize, 'left', 'top')
    end)

    section(pass, 'Stack', layout.stack, panel == 1, function(x, y, w, h)
      for i, frame in ipairs(stack) do
        if i == level then
          pass:setColor(0x6747c4)
          pass:plane(x + w / 2, y + padding + textSize / 2, 0, w - border, textSize)
        end

        pass:setColor(i == level and 0xffffff or 0x808080)
        pass:text(frame.label, x + padding, y + padding, 0, textSize, nil, 0, 'left', 'top')

        if frame.short_src then
          pass:text(frame.short_src, x + padding + stackColumnWidth + padding, y + padding, 0, textSize, nil, 0, 'left', 'top')
        end

        y = y + textSize
      end
    end)

    section(pass, 'Source', layout.source, panel == 2, function(x, y, w, h)
      if frame.lines and frame.currentline ~= -1 then
        local gutter = font:getWidth(#frame.lines) * textSize + padding / 2
        local maxlines = math.ceil((h - 2 * padding) / textSize)
        local first = math.max(1 + math.floor(sourceScroll / textSize), 1)
        local last = math.min(first + maxlines, #frame.lines)

        for i = first, last do
          local y = y + (i - 1) * textSize - sourceScroll
          if i == frame.currentline then
            pass:setColor(0x6747c4)
            pass:plane(x + w / 2, y + padding + textSize / 2, 0, w - border, textSize)
          end
          pass:setColor(i == frame.currentline and 0xffffff or 0x808080)
          pass:text(tostring(i), x + padding, y + padding, 0, textSize, nil, 0, 'left', 'top')
          pass:text(frame.lines[i], x + padding + gutter, y + padding, 0, textSize, nil, 0, 'left', 'top')
        end
      end
    end)

    section(pass, 'Variables', layout.variables, panel == 3, function(x, y, w, h)
      local edge = y + h + frame.scroll

      for i, row in ipairs(frame.rows) do
        if i == frame.rowIndex then
          pass:setColor(0x6747c4)
          pass:plane(x + w / 2, y - frame.scroll + padding + textSize / 2, 0, w - border, textSize)
        end

        pass:setColor(i == frame.rowIndex and 0xffffff or 0xc0c0c0)
        pass:text(row.name, x + padding + row.depth * padding, y - frame.scroll + padding, 0, textSize, nil, 0, 'left', 'top')
        pass:text(row.contents, x + padding + frame.columnWidth + padding, y - frame.scroll + padding, 0, textSize, nil, 0, 'left', 'top')
        y = y + textSize

        if y > edge then
          break
        end
      end
    end)
  end

  if debug then
    for i = 4, 50 do
      local frame = debug.getinfo(i, 'Snufl')

      if not frame then break end

      table.insert(stack, frame)

      frame.variables = {}
      frame.expanded = {}
      frame.rowIndex = 1
      frame.scroll = 0

      -- Pretty name
      if frame.func then
        if frame.name and lovr[frame.name] == frame.func then
          frame.label = 'lovr.' .. frame.name
        elseif frame.what == 'C' then
          for module, value in pairs(lovr) do
            if type(value) == 'table' then
              for name, fn in pairs(value) do
                if fn == frame.func then
                  frame.label = string.format('lovr.%s.%s', module, name)
                end
              end
            end
          end

          for object, methods in pairs(debug.getregistry()) do
            if type(object) == 'string' and object:match('^%u') then
              for name, fn in pairs(methods) do
                if fn == frame.func then
                  frame.label = string.format('%s:%s', object, name)
                end
              end
            end
          end
        end
      end

      frame.label = frame.label or frame.name or '<anonymous>'
      stackColumnWidth = math.max(stackColumnWidth, font:getWidth(frame.label) * textSize)

      -- Locals
      for j = 1, 100 do
        local name, value = debug.getlocal(i, j)

        if not name then
          break
        elseif name:sub(1, 1) ~= '(' then
          table.insert(frame.variables, { name, value })
        end
      end

      -- Upvalues
      for j = 1, 100 do
        local name, value = debug.getupvalue(frame.func, j)

        if not name then
          break
        else
          table.insert(frame.variables, { name, value })
        end
      end

      table.insert(frame.variables, { '_ENV', getfenv(i) })

      -- Source
      if frame.source:sub(1, 1) == '@' then
        local contents = lovr.filesystem.read(frame.source:sub(2))

        if contents then
          frame.lines = {}
          for line in contents:gmatch('([^\n]*)\n') do
            table.insert(frame.lines, line)
          end
        end
      end

      if frame.short_src == 'vector' then
        frame.short_src = '[vector]:' .. frame.currentline
      elseif frame.short_src and frame.currentline ~= -1 then
        frame.short_src = frame.short_src .. ':' .. frame.currentline
      end

      buildVariableTable(frame)
    end

    while #stack > 0 and stack[#stack].short_src and (stack[#stack].short_src:match('boot.lua') or stack[#stack].short_src:match('[C]')) do
      table.remove(stack)
    end

    if #stack == 0 then
      table.insert(stack, {
        label = '<none>',
        short_src = '',
        variables = {},
        expanded = {},
        rows = {},
        rowIndex = 1,
        scroll = 0
      })
    end

    local index = 1

    while index < #stack and stack[index].what == 'C' or (stack[index].short_src and stack[index].short_src:match('^%[vector%]')) do
      index = index + 1
    end

    -- Need to compute layout so setLevel knows where to scroll
    if lovr.headset and lovr.headset.isActive() then
      resize(lovr.headset.getDisplayDimensions())
    elseif lovr.system.isWindowOpen() then
      resize(lovr.system.getWindowDimensions())
    end

    setLevel(index)
  end

  if lovr.audio then lovr.audio.stop() end

  if not lovr.headset or lovr.headset.getPassthrough() == 'opaque' then
    lovr.graphics.setBackgroundColor(.11, .10, .14)
  else
    lovr.graphics.setBackgroundColor(0, 0, 0, 0)
  end

  if lovr.headset and lovr.headset.isActive() then
    layer = lovr.headset.newLayer(1600, 1000, { transparent = true, filter = true })
    layer:setPosition(.5, 1, -12)
    layer:setDimensions(11.2, 7)
    layer:setCurve(.1)
    lovr.headset.setLayers(layer)
    layerPass = lovr.graphics.newPass()
  end

  lovr.system.setKeyRepeat(true)
  lovr.system.setMouseMode('normal')

  return function()
    lovr.timer.step()

    local timeout = lovr.headset and lovr.headset.isActive() and 0 or .25
    lovr.system.pollEvents(timeout)

    for name, a, b, c in lovr.event.poll() do
      if name == 'quit' then
        return a or 1
      elseif name == 'restart' then
        return 'restart', lovr.restart and lovr.restart()
      elseif name == 'filechanged'then
        lovr.event.restart()
      elseif name == 'resize' then
        resize(a, b)
        dirty = true
      elseif name == 'keypressed' then
        if a == 'f5' then
          lovr.event.restart()
        elseif a == 'escape' then
          lovr.event.quit()
        elseif a == 'k' or a == 'up' then
          up()
          dirty = true
        elseif a == 'j' or a == 'down' then
          down()
          dirty = true
        elseif a == 'h' or a == 'left' then
          left()
          dirty = true
        elseif a == 'l' or a == 'right' then
          right()
          dirty = true
        elseif (a == 'g' and not lovr.system.isKeyDown('lshift', 'rshift')) or a == 'home' then
          top()
          dirty = true
        elseif (a == 'g' and lovr.system.isKeyDown('lshift', 'rshift')) or a == 'end' then
          bottom()
          dirty = true
        elseif a == 'tab' then
          focusNextPanel()
          dirty = true
        end
      elseif name == 'wheelmoved' then
        local dy = b
        local mx, my = lovr.system.getMousePosition()
        if inside(mx, my, layout.source) then
          scrollSource(sourceScroll - textSize * dy)
          dirty = true
        elseif inside(mx, my, layout.variables) then
          frame.scroll = frame.scroll - textSize * dy
          clampVariableScroll()
          dirty = true
        end
      elseif name == 'mousepressed' and c == 1 then
        local x, y = a, b
        if inside(x, y, layout.stack) then
          local row = 1 + math.floor((y - padding - layout.stack.y) / textSize)
          if row >= 1 and row <= #stack then
            setLevel(row)
          end
          panel = 1
          dirty = true
        elseif inside(x, y, layout.source) then
          panel = 2
          dirty = true
        elseif inside(x, y, layout.variables) then
          local index = 1 + math.floor((y + frame.scroll - padding - layout.variables.y) / textSize)
          if index >= 1 and index <= #frame.rows then
            local row = frame.rows[index]
            frame.rowIndex = index
            if type(row.value) == 'table' then
              frame.expanded[row.id] = not frame.expanded[row.id]
              buildVariableTable(frame)
              ensureVariableVisible()
            end
          end
          panel = 3
          dirty = true
        end
      end
    end

    if lovr.headset and lovr.headset.isActive() then
      lovr.headset.pollEvents()
      lovr.headset.update()

      for i, hand in ipairs(lovr.headset.getHands()) do
        if lovr.headset.wasPressed(hand, 'dpup') then
          up()
          dirty = true
        elseif lovr.headset.wasPressed(hand, 'dpdown') then
          down()
          dirty = true
        elseif lovr.headset.wasPressed(hand, 'dpleft') then
          left()
          dirty = true
        elseif lovr.headset.wasPressed(hand, 'dpright') then
          right()
          dirty = true
        elseif lovr.headset.wasPressed(hand, 'thumbtap') or lovr.headset.wasPressed(hand, 'trigger') then
          focusNextPanel()
          dirty = true
        end

        local dx, dy = lovr.headset.getAxis(hand, 'thumbstick')
        local magnitude = dx and math.sqrt(dx * dx + dy * dy) or 0
        local pressed = false

        if magnitude > .6 then
          if not thumbsticks[hand] then
            thumbsticks[hand] = .18
            pressed = true
          else
            thumbsticks[hand] = thumbsticks[hand] - lovr.timer.getDelta()
            if thumbsticks[hand] <= 0 then
              thumbsticks[hand] = .035
              pressed = true
            end
          end
        elseif thumbsticks[hand] and magnitude < .4 then
          thumbsticks[hand] = false
        end

        if (panel == 1 or panel == 3) and pressed then
          if math.abs(dy) > math.abs(dx) then
            if dy > 0 then
              up()
              dirty = true
            else
              down()
              dirty = true
            end
          elseif dx > 0 then
            right()
            dirty = true
          else
            left()
            dirty = true
          end
        elseif panel == 2 and math.abs(dy) > .2 then
          scrollSource(sourceScroll - (dy > 0 and dy - .2 or dy + .2) * textSize * .8)
          dirty = true
        end
      end
    end

    local window, quad

    if dirty then
      window = lovr.system.isWindowOpen() and lovr.graphics.getWindowPass()
      render(window)

      if layer and layer:getTexture() then
        quad = layerPass
        quad:setCanvas(layer:getTexture())
        quad:setClear(.11, .10, .14)
        render(quad)
      end

      dirty = false
    end

    local headset = lovr.headset and lovr.headset.isActive() and lovr.headset.getPass()

    if window or quad or headset then
      lovr.graphics.submit(window, quad, headset)

      if headset then
        lovr.headset.submit()
      end

      lovr.graphics.present()
    else
      lovr.timer.sleep(.001)
    end
  end
end
