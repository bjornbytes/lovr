-- Vibrates controllers when their triggers are pressed, draws 3D
-- models for each controller, and keeps track of controllers when
-- they are connected and disconnected.

local controllerModels = {}

function lovr.load()
  print('There are ' .. lovr.headset.getControllerCount() .. ' controllers.')
end

function lovr.update(dt)
  local controllers = lovr.headset.getControllers()

  for i, controller in ipairs(controllers) do
    if controller:getAxis('trigger') > .5 then
      controller:vibrate(.0035)
    end
  end
end

function lovr.draw()
  for controller, model in pairs(controllerModels) do
    local x, y, z = controller:getPosition()
    local angle, axisX, axisY, axisZ = controller:getOrientation()
    model:draw(x, y, z, 1, -angle, axisX, axisY, axisZ)
  end
end

function lovr.controlleradded(controller)
  print('A controller was connected!')
  controllerModels[controller] = controller:newModel()
end

function lovr.controllerremoved(controller)
  print('A controller was disconnected!')
  controllerModels[controller] = nil
end
