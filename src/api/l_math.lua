local vec2, vec3, vec4, quat, mat4 = ...

local function isvec2(t) return type(t) == 'table' and getmetatable(t) == vec2 end
local function isvec3(t) return type(t) == 'table' and getmetatable(t) == vec3 end
local function isvec4(t) return type(t) == 'table' and getmetatable(t) == vec4 end
local function isquat(t) return type(t) == 'table' and getmetatable(t) == quat end
local function ismat4(m) return type(m) == 'userdata' and getmetatable(t) == mat4 end

local EQ_THRESHOLD = 1e-10

----------------
-- vec2
----------------

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
    return rawget(v, 1)
  elseif key == 'y' then
    return rawget(v, 2)
  else
    return rawget(v, key)
  end
end

vec2.__newindex = function(v, key, val)
  if key == 'x' then
    rawset(v, 1, val)
  elseif key == 'y' then
    rawset(v, 2, val)
  else
    rawset(v, key, val)
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
    assert(isvec2(v) and isvec2(x), 'expected vector or number operands for vector addition')
    return vec2(v[1] + x[1], v[2] + x[2])
  end
end

function vec2.__sub(v, x)
  if type(x) == 'number' then
    return vec2(v[1] - x, v[2] - x)
  elseif type(v) == 'number' then
    return vec2(v - x[1], v - x[2])
  else
    assert(isvec2(v) and isvec2(x), 'expected vector or number operands for vector subtraction')
    return vec2(v[1] - x[1], v[2] - x[2])
  end
end

function vec2.__mul(v, x)
  if type(x) == 'number' then
    return vec2(v[1] * x, v[2] * x)
  elseif type(v) == 'number' then
    return vec2(v * x[1], v * x[2])
  else
    assert(isvec2(v) and isvec2(x), 'expected vector or number operands for vector multiplication')
    return vec2(v[1] * x[1], v[2] * x[2])
  end
end

function vec2.__div(v, x)
  if type(x) == 'number' then
    return vec2(v[1] / x, v[2] / x)
  elseif type(v) == 'number' then
    return vec2(v / x[1], v / x[2])
  else
    assert(isvec2(v) and isvec2(x), 'expected vector or number operands for vector division')
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

----------------
-- vec3
----------------

setmetatable(vec3, {
  __call = function(t, x, y, z)
    if isvec3(x) then
      return setmetatable({ x[1], x[2], x[3] }, vec3)
    elseif not x or type(x) == 'number' then
      x = x or 0
      return setmetatable({ x, y or x, z or x }, vec3)
    else
      return setmetatable({ 0, 0, 0 }, vec3):set(x, y, z)
    end
  end
})

vec3.zero = vec3(0, 0, 0)
vec3.one = vec3(1, 1, 1)
vec3.left = vec3(-1, 0, 0)
vec3.right = vec3(1, 0, 0)
vec3.up = vec3(0, 1, 0)
vec3.down = vec3(0, -1, 0)
vec3.back = vec3(0, 0, 1)
vec3.forward = vec3(0, 0, -1)

vec3.__index = function(v, key)
  local val = rawget(vec3, key)
  if val then
    return val
  elseif key == 'x' then
    return rawget(v, 1)
  elseif key == 'y' then
    return rawget(v, 2)
  elseif key == 'z' then
    return rawget(v, 3)
  else
    return rawget(v, key)
  end
end

vec3.__newindex = function(v, key, val)
  if key == 'x' then
    rawset(v, 1, val)
  elseif key == 'y' then
    rawset(v, 2, val)
  elseif key == 'z' then
    rawset(v, 3, val)
  else
    rawset(v, key, val)
  end
end

function vec3.__tostring(v)
  return ('(%f, %f, %f)'):format(v[1], v[2], v[3])
end

function vec3.__add(v, x)
  if type(x) == 'number' then
    return vec3(v[1] + x, v[2] + x, v[3] + x)
  elseif type(v) == 'number' then
    return vec3(v + x[1], v + x[2], v + x[3])
  else
    assert(isvec3(v) and isvec3(x), 'expected vector or number operands for vector addition')
    return vec3(v[1] + x[1], v[2] + x[2], v[3] + x[3])
  end
end

function vec3.__sub(v, x)
  if type(x) == 'number' then
    return vec3(v[1] - x, v[2] - x, v[3] - x)
  elseif type(v) == 'number' then
    return vec3(v - x[1], v - x[2], v - x[3])
  else
    assert(isvec3(v) and isvec3(x), 'expected vector or number operands for vector subtraction')
    return vec3(v[1] - x[1], v[2] - x[2], v[3] - x[3])
  end
end

function vec3.__mul(v, x)
  if type(x) == 'number' then
    return vec3(v[1] * x, v[2] * x, v[3] * x)
  elseif type(v) == 'number' then
    return vec3(v * x[1], v * x[2], v * x[3])
  else
    assert(isvec3(v) and isvec3(x), 'expected vector or number operands for vector multiplication')
    return vec3(v[1] * x[1], v[2] * x[2], v[3] * x[3])
  end
end

function vec3.__div(v, x)
  if type(x) == 'number' then
    return vec3(v[1] / x, v[2] / x, v[3] / x)
  elseif type(v) == 'number' then
    return vec3(v / x[1], v / x[2], v / x[3])
  else
    assert(isvec3(v) and isvec3(x), 'expected vector or number operands for vector division')
    return vec3(v[1] / x[1], v[2] / x[2], v[3] / x[3])
  end
end

function vec3.__unm(v)
  return vec3(-v[1], -v[2], -v[3])
end

function vec3.__len(v)
  return v:length()
end

function vec3.type()
  return 'Vec3'
end

function vec3.equals(v, u)
  return v:distance(u) < EQ_THRESHOLD
end

function vec3.unpack(v)
  return v[1], v[2], v[3]
end

function vec3.set(v, x, y, z)
  if isvec3(x) then
    v[1], v[2], v[3] = x[1], x[2], x[3]
  elseif x == nil or type(x) == 'number' then
    x = x or 0
    v[1], v[2], v[3] = x, y or x, z or x
  elseif isquat(x) then
    v[1], v[2], v[3] = x:direction()
  elseif ismat4(x) then
    v[1], v[2], v[3] = x:getPosition()
  end
  return v
end

function vec3.add(v, x, y, z)
  if isvec3(x) then x, y, z = x[1], x[2], x[3] end
  v[1], v[2], v[3] = v[1] + x, v[2] + (y or x), v[3] + (z or x)
  return v
end

function vec3.sub(v, x, y, z)
  if isvec3(x) then x, y, z = x[1], x[2], x[3] end
  v[1], v[2], v[3] = v[1] - x, v[2] - (y or x), v[3] - (z or x)
  return v
end

function vec3.mul(v, x, y, z)
  if isvec3(x) then x, y, z = x[1], x[2], x[3] end
  v[1], v[2], v[3] = v[1] * x, v[2] * (y or x), v[3] * (z or x)
  return v
end

function vec3.div(v, x, y, z)
  if isvec3(x) then x, y, z = x[1], x[2], x[3] end
  v[1], v[2], v[3] = v[1] / x, v[2] / (y or x), v[3] / (z or x)
  return v
end

function vec3.length(v)
  return math.sqrt(v[1] * v[1] + v[2] * v[2] + v[3] * v[3])
end

function vec3.normalize(v)
  local length = v:length()
  if length > 0 then
    v[1], v[2], v[3] = v[1] / length, v[2] / length, v[3] / length
  end
  return v
end

function vec3.distance(v, x, y, z)
  if isvec3(x) then x, y, z = x[1], x[2], x[3] end
  local dx, dy, dz = v[1] - x, v[2] - y, v[3] - z
  return math.sqrt(dx * dx + dy * dy + dz * dz)
end

function vec3.dot(v, x, y, z)
  if isvec3(x) then x, y, z = x[1], x[2], x[3] end
  return v[1] * x + v[2] * y + v[3] * z
end

function vec3.cross(v, x, y, z)
  if isvec3(x) then x, y, z = x[1], x[2], x[3] end
  local cx = v[2] * z - v[3] * y
  local cy = v[3] * x - v[1] * z
  local cz = v[1] * y - v[2] * x
  v[1], v[2], v[3] = cx, cy, cz
  return v
end

function vec3.lerp(v, x, y, z, t)
  if isvec3(x) then x, y, z, t = x[1], x[2], x[3], y end
  v[1] = v[1] * (1 - t) + x * t
  v[2] = v[2] * (1 - t) + y * t
  v[3] = v[3] * (1 - t) + z * t
  return v
end

function vec3.angle(v, x, y, z)
  if isvec3(x) then x, y, z = x[1], x[2], x[3] end
  local denom = v:length() * math.sqrt(x * x + y * y + z * z)
  if denom == 0 then
    return math.pi / 2
  else
    local cos = (v[1] * x + v[2] * y + v[3] * z) / denom
    if cos < -1 then cos = -1
    elseif cos > 1 then cos = 1 end
    return math.acos(cos)
  end
end

function vec3.transform(v, m)
  return v:set(m * v)
end

function vec3.rotate(v, q)
  return v:set(q * v)
end

----------------
-- vec4
----------------

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
    return rawget(v, 1)
  elseif key == 'y' then
    return rawget(v, 2)
  elseif key == 'z' then
    return rawget(v, 3)
  elseif key == 'w' then
    return rawget(v, 4)
  else
    return rawget(v, key)
  end
end

vec4.__newindex = function(v, key, val)
  if key == 'x' then
    rawset(v, 1, val)
  elseif key == 'y' then
    rawset(v, 2, val)
  elseif key == 'z' then
    rawset(v, 3, val)
  elseif key == 'w' then
    rawset(v, 4, val)
  else
    rawset(v, key, val)
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
    assert(isvec4(v) and isvec4(x), 'expected vector or number operands for vector addition')
    return vec4(v[1] + x[1], v[2] + x[2], v[3] + x[3], v[4] + x[4])
  end
end

function vec4.__sub(v, x)
  if type(x) == 'number' then
    return vec4(v[1] - x, v[2] - x, v[3] - x, v[4] - x)
  elseif type(v) == 'number' then
    return vec4(v - x[1], v - x[2], v - x[3], v - x[4])
  else
    assert(isvec4(v) and isvec4(x), 'expected vector or number operands for vector subtraction')
    return vec4(v[1] - x[1], v[2] - x[2], v[3] - x[3], v[4] - x[4])
  end
end

function vec4.__mul(v, x)
  if type(x) == 'number' then
    return vec4(v[1] * x, v[2] * x, v[3] * x, v[4] * x)
  elseif type(v) == 'number' then
    return vec4(v * x[1], v * x[2], v * x[3], v * x[4])
  else
    assert(isvec4(v) and isvec4(x), 'expected vector or number operands for vector multiplication')
    return vec4(v[1] * x[1], v[2] * x[2], v[3] * x[3], v[4] * x[4])
  end
end

function vec4.__div(v, x)
  if type(x) == 'number' then
    return vec4(v[1] / x, v[2] / x, v[3] / x, v[4] / x)
  elseif type(v) == 'number' then
    return vec4(v / x[1], v / x[2], v / x[3], v / x[4])
  else
    assert(isvec4(v) and isvec4(x), 'expected vector or number operands for vector division')
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

function vec4.transform(v, m)
  return v:set(m * v)
end

----------------
-- quat
----------------

quat.__index = quat

local function quat_fromAngleAxis(angle, ax, ay, az)
  local s, c = math.sin(angle * .5), math.cos(angle * .5)

  local length = math.sqrt(ax * ax + ay * ay + az * az)
  if length > 0 then s = s / length end

  return ax * s, ay * s, az * s, c
end

local function quat_between(u, v)
  local dot = u:dot(v)
  if dot > .99999 then
    return 0, 0, 0, 1
  elseif dot < -.99999 then
    local axis = vec3(1, 0, 0):cross(u)
    if #axis < .00001 then axis = vec3(0, 1, 0):cross(u) end
    return quat_fromAngleAxis(math.pi, axis:unpack())
  else
    local x, y, z = u[2] * v[3] - u[3] * v[2], u[3] * v[1] - u[1] * v[3], u[1] * v[2] - u[2] * v[1]
    local w = 1 + dot
    local length = (x * x + y * y + z * z + w * w) ^ .5
    if length == 0 then
      return x, y, z, w
    else
      return x / length, y / length, z / length, w / length
    end
  end
end

local function quat_mulVec3(q, v)
  local cx, cy, cz = q[2] * v[3] - q[3] * v[2], q[3] * v[1] - q[1] * v[3], q[1] * v[2] - q[2] * v[1]
  local q_q = q[1] * q[1] + q[2] * q[2] + q[3] * q[3]
  local q_v = q[1] * v[1] + q[2] * v[2] + q[3] * v[3]
  return vec3(
    v[1] * (q[3] * q[3] - q_q) + q[1] * 2 * q_v + cx * 2 * q[3],
    v[2] * (q[3] * q[3] - q_q) + q[2] * 2 * q_v + cy * 2 * q[3],
    v[3] * (q[3] * q[3] - q_q) + q[3] * 2 * q_v + cz * 2 * q[3]
  )
end

local function quat_mulQuat(q, r)
  return
    q[1] * r[4] + q[4] * r[1] + q[2] * r[3] - q[3] * r[2],
    q[2] * r[4] + q[4] * r[2] + q[3] * r[1] - q[1] * r[3],
    q[3] * r[4] + q[4] * r[3] + q[1] * r[2] - q[2] * r[1],
    q[4] * r[4] - q[1] * r[1] - q[2] * r[2] - q[3] * r[3]
end

setmetatable(quat, {
  __call = function(t, x, y, z, w, raw)
    return quat.set(setmetatable({}, quat), x, y, z, w, raw)
  end
})

function quat.__tostring(q)
  return ('(%f, %f, %f, %f)'):format(q[1], q[2], q[3], q[4])
end

function quat.__mul(q, x)
  if isquat(q) then
    if isvec3(x) then
      return quat_mulVec3(q, x)
    elseif isquat(x) then
      return setmetatable({ quat_mulQuat(q, x) }, quat)
    end
  end
end

function quat.__unm(q)
  return quat(-q[1], -q[2], -q[3], q[4])
end

function quat.__len(q)
  return q:length()
end

function quat.type()
  return 'Quat'
end

function quat.equals(q, r)
  return math.abs(q[1] * r[1] + q[2] * r[2] + q[3] * r[3] + q[4] * r[4]) >= 1 - 1e-5
end

function quat.unpack(q, raw)
  if raw then
    return q[1], q[2], q[3], q[4]
  else
    local length = q:length()
    local x, y, z, w = q[1], q[2], q[3], q[4]
    if length > 0 then x, y, z, w = x / length, y / length, z / length, w / length end

    local s = math.sqrt(1 - w * w)
    s = s < .0001 and 1 or 1 / s

    return 2 * math.acos(w), x * s, y * s, z * s
  end
end

function quat.set(q, x, y, z, w, raw)
  if x == nil then
    q[1], q[2], q[3], q[4] = 0, 0, 0, 1
  elseif type(x) == 'number' then
    if raw then
      q[1], q[2], q[3], q[4] = x, y, z, w
    else
      q[1], q[2], q[3], q[4] = quat_fromAngleAxis(x, y, z, w)
    end
  elseif isquat(x) then
    q[1], q[2], q[3], q[4] = x[1], x[2], x[3], x[4]
  elseif isvec3(x) then
    if isvec3(y) then
      q[1], q[2], q[3], q[4] = quat_between(x, y)
    else
      q[1], q[2], q[3], q[4] = quat_between(vec3.forward, x)
    end
  elseif ismat4(x) then
    q[1], q[2], q[3], q[4] = quat_fromAngleAxis(x:getOrientation())
  end

  return q
end

function quat.mul(q, x)
  if isvec3(x) then
    return quat_mulVec3(q, x)
  elseif isquat(x) then
    q[1], q[2], q[3], q[4] = quat_mulQuat(q, x)
    return q
  else
    error('Expected Vec3 or Quat for Quat:mul')
  end
end

function quat.length(q)
  return math.sqrt(q[1] * q[1] + q[2] * q[2] + q[3] * q[3] + q[4] * q[4])
end

function quat.normalize(q)
  local length = q:length()
  if length > 0 then
    q[1], q[2], q[3], q[4] = q[1] / length, q[2] / length, q[3] / length, q[4] / length
  end
  return q
end

function quat.direction(q)
  local x = -2 * q[1] * q[3] - 2 * q[4] * q[2]
  local y = -2 * q[2] * q[3] + 2 * q[4] * q[1]
  local z = -1 + 2 * q[1] * q[1] + 2 * q[2] * q[2]
  return vec3(x, y, z)
end

function quat.conjugate(q)
  q[1] = -q[1]
  q[2] = -q[2]
  q[3] = -q[3]
  return q
end

function quat.slerp(q, r, t)
  assert(isquat(r), 'Expected Quat for Quat:slerp')
  local dot = q[1] * r[1] + q[2] * r[2] + q[3] * r[3] + q[4] * r[4]

  if math.abs(dot) >= 1 then
    return q
  end

  local x, y, z, w = q[1], q[2], q[3], q[4]

  if dot < 0 then
    x, y, z, w, dot = -x, -y, -z, -w, -dot
  end

  local halfTheta = math.acos(dot)
  local sinHalfTheta = math.sqrt(1 - dot * dot)

  if math.abs(sinHalfTheta) < .001 then
    q[1] = x * .5 + r[1] * .5
    q[2] = y * .5 + r[2] * .5
    q[3] = z * .5 + r[3] * .5
    q[4] = w * .5 + r[4] * .5
    return q
  end

  local a = math.sin((1 - t) * halfTheta) / sinHalfTheta
  local b = math.sin(t * halfTheta) / sinHalfTheta

  q[1] = x * a + r[1] * b
  q[2] = y * a + r[2] * b
  q[3] = z * a + r[3] * b
  q[4] = w * a + r[4] * b
  return q
end

function quat.getEuler(q)
  local unit = q[1] * q[1] + q[2] * q[2] + q[3] * q[3] + q[4] * q[4]
  local test = q[1] * q[4] - q[2] * q[3]

  if test > (.5 - 1e-7) * unit then
    return math.pi / 2, 2 * math.atan2(q[2], q[1]), 0
  elseif test < -(.5 - eps) * unit then
    return -math.pi / 2, -2 * math.atan2(q[2], q[1]), 0
  else
    return math.asin(2 * test),
      math.atan2(2 * q[4] * q[2] + 2 * q[3] * q[1], 1 - 2 * (q[1] * q[1] + q[2] * q[2])),
      math.atan2(2 * q[4] * q[3] + 2 * q[1] * q[2], 1 - 2 * (q[3] * q[3] + q[1] * q[1]))
  end
end

function quat.setEuler(q, x, y, z)
  local cx = math.cos(x * .5)
  local sx = math.sin(x * .5)
  local cy = math.cos(y * .5)
  local sy = math.sin(y * .5)
  local cz = math.cos(z * .5)
  local sz = math.sin(z * .5)
  q[1] = cy * sx * cz + sy * cx * sz
  q[2] = sy * cx * cz - cy * sx * sz
  q[3] = cy * cx * sz - sy * sx * cz
  q[4] = cy * cx * cz + sy * sx * sz
  return q
end
