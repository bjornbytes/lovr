function lovr.load()
  if not lovr.graphics then
    print(string.format('LÖVR %d.%d.%d\nNo game', lovr.getVersion()))
    lovr.event.quit()
    return
  end

  if not lovr.headset or lovr.headset.getPassthrough() == 'opaque' then
    lovr.graphics.setBackgroundColor(0x20232c)
  end

  logo = lovr.graphics.newShader('unlit', 'logo.spv')
end

function lovr.draw(pass)
  local padding = .1
  local font = lovr.graphics.getDefaultFont()
  local fade = .315 + .685 * math.abs(math.sin(lovr.headset.getTime() * 2))
  local titlePosition = 1.5 - padding
  local subtitlePosition = titlePosition - font:getHeight() * .25 - padding

  pass:setFaceCull(true)
  pass:setShader(logo)
  pass:plane(0, 2, -3)
  pass:setShader()

  pass:text('LÖVR', -.012, titlePosition, -3, .25, quat(0, 0, 1, 0), nil, 'center', 'top')

  pass:setColor(.9, .9, .9, fade)
  pass:text('No game :(', -.005, subtitlePosition, -3, .15, 0, 0, 1, 0, nil, 'center', 'top')
end

function lovr.keypressed(key)
  if key == 'escape' then
    lovr.event.quit()
  end
end
