-- Draws a spinning cube

function lovr.draw()
  local x, y, z = 0, 0, -1
  local size = .5
  local angle = lovr.timer.getTime()
  lovr.graphics.cube('line', x, y, z, size, angle)
end
