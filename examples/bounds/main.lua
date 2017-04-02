-- Draws a rectangle on the floor, coloring it red
-- when the player is close to the virtual boundary.

function lovr.load()
  if not lovr.headset.isPresent() then
    error('This example needs a headset')
  end

  bounds = lovr.graphics.newMesh(lovr.headset.getBounds())
end

function lovr.draw()
  if lovr.headset.isBoundsVisible() then
    lovr.graphics.setColor(255, 0, 0)
  else
    lovr.graphics.setColor(255, 255, 255)
  end

  bounds:draw()
end
