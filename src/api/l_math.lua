local lovr_math = ...

local EQ_THRESHOLD = 1e-10

local vec2 = {}
local vec4 = {}

----------------
-- vec2
----------------

local function isvec2(t)
  return type(t) == 'table' and getmetatable(t) == vec2
end

setmetatable(vec2, {
  __call = function(t, x, y)
    if isvec2(x) then
      return setmetatable({ x[1], x[2] }, vec2)
    else
      x = x or 0
      return setmetatable({ x, y or x }, vec2)
    end
  end
})

vec2.zero = vec2(0, 0)
vec2.one = vec2(1, 1)

vec2.__index = function(v, key)
  local val = rawget(vec2, key)
  if val then
    return val
  elseif key == 'x' then
    return v[1]
  elseif key == 'y' then
    return v[2]
  end
end

vec2.__newindex = function(v, key, val)
  if key == 'x' then
    v[1] = val
  elseif key == 'y' then
    v[2] = val
  end
end

function vec2.__tostring(v)
  return ('(%f, %f)'):format(v[1], v[2])
end

function vec2.__add(v, x)
  if type(x) == 'number' then
    return vec2(v[1] + x, v[2] + x)
  elseif type(v) == 'number' then
    return vec2(v + x[1], v + x[2])
  else
    return vec2(v[1] + x[1], v[2] + x[2])
  end
end

function vec2.__sub(v, x)
  if type(x) == 'number' then
    return vec2(v[1] - x, v[2] - x)
  elseif type(v) == 'number' then
    return vec2(v - x[1], v - x[2])
  else
    return vec2(v[1] - x[1], v[2] - x[2])
  end
end

function vec2.__mul(v, x)
  if type(x) == 'number' then
    return vec2(v[1] * x, v[2] * x)
  elseif type(v) == 'number' then
    return vec2(v * x[1], v * x[2])
  else
    return vec2(v[1] * x[1], v[2] * x[2])
  end
end

function vec2.__div(v, x)
  if type(x) == 'number' then
    return vec2(v[1] / x, v[2] / x)
  elseif type(v) == 'number' then
    return vec2(v / x[1], v / x[2])
  else
    return vec2(v[1] / x[1], v[2] / x[2])
  end
end

function vec2.__unm(v)
  return vec2(-v[1], -v[2])
end

function vec2.__len(v)
  return v:length()
end

function vec2.type()
  return 'Vec2'
end

function vec2.equals(v, u)
  return v:distance(u) < EQ_THRESHOLD
end

function vec2.unpack(v)
  return v[1], v[2]
end

function vec2.set(v, x, y)
  if isvec2(x) then
    v[1], v[2] = x[1], x[2]
  else
    x = x or 0
    v[1], v[2] = x, y or x
  end
  return v
end

function vec2.add(v, x, y)
  if isvec2(x) then x, y = x[1], x[2] end
  v[1], v[2] = v[1] + x, v[2] + (y or x)
  return v
end

function vec2.sub(v, x, y)
  if isvec2(x) then x, y = x[1], x[2] end
  v[1], v[2] = v[1] - x, v[2] - (y or x)
  return v
end

function vec2.mul(v, x, y)
  if isvec2(x) then x, y = x[1], x[2] end
  v[1], v[2] = v[1] * x, v[2] * (y or x)
  return v
end

function vec2.div(v, x, y)
  if isvec2(x) then x, y = x[1], x[2] end
  v[1], v[2] = v[1] / x, v[2] / (y or x)
  return v
end

function vec2.length(v)
  return math.sqrt(v[1] * v[1] + v[2] * v[2])
end

function vec2.normalize(v)
  local length = v:length()
  if length > 0 then
    v[1], v[2] = v[1] / length, v[2] / length
  end
  return v
end

function vec2.distance(v, x, y)
  if isvec2(x) then x, y = x[1], x[2] end
  local dx, dy = v[1] - x, v[2] - y
  return math.sqrt(dx * dx + dy * dy)
end

function vec2.dot(v, x, y)
  if isvec2(x) then x, y = x[1], x[2] end
  return v[1] * x + v[2] * y
end

function vec2.lerp(v, x, y, t)
  if isvec2(x) then x, y, t = x[1], x[2], y end
  v[1] = v[1] * (1 - t) + x * t
  v[2] = v[2] * (1 - t) + y * t
  return v
end

function vec2.angle(v, x, y)
  if isvec2(x) then x, y = x[1], x[2] end
  local denom = v:length() * math.sqrt(x * x + y * y)
  if denom == 0 then
    return math.pi / 2
  else
    local cos = (v[1] * x + v[2] * y) / denom
    if cos < -1 then cos = -1
    elseif cos > 1 then cos = 1 end
    return math.acos(cos)
  end
end

lovr_math.vec2 = vec2
lovr_math.newVec2 = vec2

----------------
-- vec4
----------------

local function isvec4(t)
  return type(t) == 'table' and getmetatable(t) == vec4
end

setmetatable(vec4, {
  __call = function(t, x, y, z, w)
    if isvec4(x) then
      return setmetatable({ x[1], x[2], x[3], x[4] }, vec4)
    else
      x = x or 0
      return setmetatable({ x, y or x, z or x, w or x }, vec4)
    end
  end
})

vec4.zero = vec4(0, 0, 0, 0)
vec4.one = vec4(1, 1, 1, 1)

vec4.__index = function(v, key)
  local val = rawget(vec4, key)
  if val then
    return val
  elseif key == 'x' then
    return v[1]
  elseif key == 'y' then
    return v[2]
  elseif key == 'z' then
    return v[3]
  elseif key == 'w' then
    return v[4]
  end
end

vec4.__newindex = function(v, key, val)
  if key == 'x' then
    v[1] = val
  elseif key == 'y' then
    v[2] = val
  elseif key == 'z' then
    v[3] = val
  elseif key == 'w' then
    v[4] = val
  end
end

function vec4.__tostring(v)
  return ('(%f, %f, %f, %f)'):format(v[1], v[2], v[3], v[4])
end

function vec4.__add(v, x)
  if type(x) == 'number' then
    return vec4(v[1] + x, v[2] + x, v[3] + x, v[4] + x)
  elseif type(v) == 'number' then
    return vec4(v + x[1], v + x[2], v + x[3], v + x[4])
  else
    return vec4(v[1] + x[1], v[2] + x[2], v[3] + x[3], v[4] + x[4])
  end
end

function vec4.__sub(v, x)
  if type(x) == 'number' then
    return vec4(v[1] - x, v[2] - x, v[3] - x, v[4] - x)
  elseif type(v) == 'number' then
    return vec4(v - x[1], v - x[2], v - x[3], v - x[4])
  else
    return vec4(v[1] - x[1], v[2] - x[2], v[3] - x[3], v[4] - x[4])
  end
end

function vec4.__mul(v, x)
  if type(x) == 'number' then
    return vec4(v[1] * x, v[2] * x, v[3] * x, v[4] * x)
  elseif type(v) == 'number' then
    return vec4(v * x[1], v * x[2], v * x[3], v * x[4])
  else
    return vec4(v[1] * x[1], v[2] * x[2], v[3] * x[3], v[4] * x[4])
  end
end

function vec4.__div(v, x)
  if type(x) == 'number' then
    return vec4(v[1] / x, v[2] / x, v[3] / x, v[4] / x)
  elseif type(v) == 'number' then
    return vec4(v / x[1], v / x[2], v / x[3], v / x[4])
  else
    return vec4(v[1] / x[1], v[2] / x[2], v[3] / x[3], v[4] / x[4])
  end
end

function vec4.__unm(v)
  return vec4(-v[1], -v[2], -v[3], -v[4])
end

function vec4.__len(v)
  return v:length()
end

function vec4.type()
  return 'Vec4'
end

function vec4.equals(v, u)
  return v:distance(u) < EQ_THRESHOLD
end

function vec4.unpack(v)
  return v[1], v[2], v[3], v[4]
end

function vec4.set(v, x, y, z, w)
  if isvec4(x) then
    v[1], v[2], v[3], v[4] = x[1], x[2], x[3], x[4]
  else
    x = x or 0
    v[1], v[2], v[3], v[4] = x, y or x, z or x, w or x
  end
  return v
end

function vec4.add(v, x, y, z, w)
  if isvec4(x) then x, y, z, w = x[1], x[2], x[3], x[4] end
  v[1], v[2], v[3], v[4] = v[1] + x, v[2] + (y or x), v[3] + (z or x), v[4] + (w or x)
  return v
end

function vec4.sub(v, x, y, z, w)
  if isvec4(x) then x, y, z, w = x[1], x[2], x[3], x[4] end
  v[1], v[2], v[3], v[4] = v[1] - x, v[2] - (y or x), v[3] - (z or x), v[4] - (w or x)
  return v
end

function vec4.mul(v, x, y, z, w)
  if isvec4(x) then x, y, z, w = x[1], x[2], x[3], x[4] end
  v[1], v[2], v[3], v[4] = v[1] * x, v[2] * (y or x), v[3] * (z or x), v[4] * (w or x)
  return v
end

function vec4.div(v, x, y, z, w)
  if isvec4(x) then x, y, z, w = x[1], x[2], x[3], x[4] end
  v[1], v[2], v[3], v[4] = v[1] / x, v[2] / (y or x), v[3] / (z or x), v[4] / (w or x)
  return v
end

function vec4.length(v)
  return math.sqrt(v[1] * v[1] + v[2] * v[2] + v[3] * v[3] + v[4] * v[4])
end

function vec4.normalize(v)
  local length = v:length()
  if length > 0 then
    v[1], v[2], v[3], v[4] = v[1] / length, v[2] / length, v[3] / length, v[4] / length
  end
  return v
end

function vec4.distance(v, x, y, z, w)
  if isvec4(x) then x, y, z, w = x[1], x[2], x[3], x[4] end
  local dx, dy, dz, dw = v[1] - x, v[2] - y, v[3] - z, v[4] - w
  return math.sqrt(dx * dx + dy * dy + dz * dz + dw * dw)
end

function vec4.dot(v, x, y, z, w)
  if isvec4(x) then x, y, z, w = x[1], x[2], x[3], x[4] end
  return v[1] * x + v[2] * y + v[3] * z + v[4] * w
end

function vec4.lerp(v, x, y, z, w, t)
  if isvec4(x) then x, y, z, w, t = x[1], x[2], x[3], x[4], y end
  v[1] = v[1] * (1 - t) + x * t
  v[2] = v[2] * (1 - t) + y * t
  v[3] = v[3] * (1 - t) + z * t
  v[4] = v[4] * (1 - t) + w * t
  return v
end

function vec4.angle(v, x, y, z, w)
  if isvec4(x) then x, y, z, w = x[1], x[2], x[3], x[4] end
  local denom = v:length() * math.sqrt(x * x + y * y + z * z + w * w)
  if denom == 0 then
    return math.pi / 2
  else
    local cos = (v[1] * x + v[2] * y + v[3] * z + v[4] * w) / denom
    if cos < -1 then cos = -1
    elseif cos > 1 then cos = 1 end
    return math.acos(cos)
  end
end

lovr_math.vec4 = vec4
lovr_math.newVec4 = vec4
