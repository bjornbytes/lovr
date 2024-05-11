local lovr_math = ...

local EQ_THRESHOLD = 1e-10

local vec2 = {}
vec2.__index = vec2
setmetatable(vec2, {
  __call = function(t, x, y)
    if type(x) == 'table' then
      return setmetatable({ x[1], x[2] }, vec2)
    else
      return setmetatable({ x, y or x }, vec2)
    end
  end
})

local function isvec2(t)
  return type(t) == 'table' and getmetatable(t) == vec2
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
    v[1], v[2] = x, y or x
  end
  return v
end

function vec2.add(v, x, y)
  if isvec2(x) then
    v[1], v[2] = v[1] + x[1], v[2] + x[2]
  else
    v[1], v[2] = v[1] + x, v[2] + (y or x)
  end
  return v
end

function vec2.sub(v, x, y)
  if isvec2(x) then
    v[1], v[2] = v[1] - x[1], v[2] - x[2]
  else
    v[1], v[2] = v[1] - x, v[2] - (y or x)
  end
  return v
end

function vec2.mul(v, x, y)
  if isvec2(x) then
    v[1], v[2] = v[1] * x[1], v[2] * x[2]
  else
    v[1], v[2] = v[1] * x, v[2] * (y or x)
  end
  return v
end

function vec2.div(v, x, y)
  if isvec2(x) then
    v[1], v[2] = v[1] / x[1], v[2] / x[2]
  else
    v[1], v[2] = v[1] / x, v[2] / (y or x)
  end
  return v
end

function vec2.length(v)
  return math.sqrt(v[1] * v[1] + v[2] * v[2] + v[3] * v[3])
end

function vec2.normalize(v)
  local length = v:length()
  if length > 0 then
    v[1], v[2], v[3] = v[1] / length, v[2] / length, v[3] / length
  end
  return v
end

function vec2.distance(v, x, y)
  if isvec2(x) then
    local dx, dy = v[1] - x[1], v[2] - x[2]
    return math.sqrt(dx * dx + dy * dy)
  else

  end
end

lovr_math.vec2 = vec2
