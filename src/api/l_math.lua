local lovr_math = ...

local EQ_THRESHOLD = 1e-10

local vec2 = {}

local function isvec2(t)
  return type(t) == 'table' and getmetatable(t) == vec2
end

setmetatable(vec2, {
  __call = function(t, x, y)
    if isvec2(x) then
      return setmetatable({ x[1], x[2] }, vec2)
    else
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
    v[1], v[2] = x or 0, y or x or 0
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
