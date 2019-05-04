local function parseVersion(version)
  return version:match('(%d+)%.(%d+)%.(%d+)')
end

local targetMajor, targetMinor, targetPatch = parseVersion(...)

local shims = {
  ['master'] = function()
    --
  end
}

for version, shim in pairs(shims) do
  if version == 'master' then
    shim()
  else
    local major, minor, patch = parseVersion(version)
    if major > targetMajor or
      (major == targetMajor and minor > targetMinor) or
      (major == targetMajor and minor == targetMinor and patch >= targetPatch)
    then
      shim()
    end
  end
end
