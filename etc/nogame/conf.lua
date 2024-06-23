if lovr.filesystem.getRealDirectory('main.lua') ~= lovr.filesystem.getBundlePath() then
  return -- Only run this conf file if bundled with the executable
end

function lovr.conf(t)
  t.headset.supersample = true
  t.modules.audio = false
  t.modules.physics = false
end
