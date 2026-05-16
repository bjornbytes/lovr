local abs, sqrt, sin, cos, asin, acos, atan2 = math.abs, math.sqrt, math.sin, math.cos, math.asin, math.acos, math.atan2

vector = {}
vector.__index = vector

function vector.pack(x, y, z)
  local v = { x = x, y = y or x, z = z or (y and 0 or x) }
  setmetatable(v, vector)
  return v
end

function vector.unpack(v)
  return v.x, v.y, v.z
end

function vector.length(v)
  return sqrt(v.x * v.x + v.y * v.y + v.z * v.z)
end

function vector.normalize(v)
  local x, y, z = v.x, v.y, v.z
  local length = sqrt(x * x + y * y + z * z)

  if length == 0 then
    return v
  end

  local normalized = { x = x / length, y = y / length, z = z / length }
  setmetatable(normalized, vector)
  return normalized
end

function vector.distance(v, u)
  local dx, dy, dz = v.x - u.x, v.y - u.y, v.z - u.z
  return sqrt(dx * dx + dy * dy + dz * dz)
end

function vector.cross(v, u)
  local cross = {
    x = v.y * u.z - v.z * u.y,
    y = v.z * u.x - v.x * u.z,
    z = v.x * u.y - v.y * u.x
  }

  setmetatable(cross, vector)
  return cross
end

function vector.dot(v, u)
  return v.x * u.x + v.y * u.y + v.z * u.z
end

function vector.angle(v, u, axis)
  local cx, cy, cz = v.y * u.z - v.z * u.y, v.z * u.x - v.x * u.z, v.x * u.y - v.y * u.x
  local sina = sqrt(cx * cx + cy * cy + cz * cz)
  local cosa = v.x * u.x + v.y * u.y + v.z * u.z
  local angle = atan2(sina, cosa)

  if axis and cx * axis.x + cy * axis.y + cz * axis.z < 0 then
    angle = -angle
  end

  return angle
end

function vector.lerp(v, u, t)
  local lerped = {
    x = v.x + (u.x - v.x) * t,
    y = v.y + (u.y - v.y) * t,
    z = v.z + (u.z - v.z) * t
  }

  setmetatable(lerped, vector)
  return lerped
end

-- Deprecated
function vector.rotate(v, q)
  return q * v
end

function vector.__add(a, b)
  local x, y, z

  if type(a) == 'number' then
    x = a + b.x
    y = a + b.y
    z = a + b.z
  elseif type(b) == 'number' then
    x = a.x + b
    y = a.y + b
    z = a.z + b
  else
    x = a.x + b.x
    y = a.y + b.y
    z = a.z + b.z
  end

  local v = { x = x, y = y, z = z }
  setmetatable(v, vector)
  return v
end

function vector.__sub(a, b)
  local x, y, z

  if type(a) == 'number' then
    x = a - b.x
    y = a - b.y
    z = a - b.z
  elseif type(b) == 'number' then
    x = a.x - b
    y = a.y - b
    z = a.z - b
  else
    x = a.x - b.x
    y = a.y - b.y
    z = a.z - b.z
  end

  local v = { x = x, y = y, z = z }
  setmetatable(v, vector)
  return v
end

function vector.__mul(a, b)
  local x, y, z

  if type(a) == 'number' then
    x = a * b.x
    y = a * b.y
    z = a * b.z
  elseif type(b) == 'number' then
    x = a.x * b
    y = a.y * b
    z = a.z * b
  else
    x = a.x * b.x
    y = a.y * b.y
    z = a.z * b.z
  end

  local v = { x = x, y = y, z = z }
  setmetatable(v, vector)
  return v
end

function vector.__div(a, b)
  local x, y, z

  if type(a) == 'number' then
    x = a / b.x
    y = a / b.y
    z = a / b.z
  elseif type(b) == 'number' then
    x = a.x / b
    y = a.y / b
    z = a.z / b
  else
    x = a.x / b.x
    y = a.y / b.y
    z = a.z / b.z
  end

  local v = { x = x, y = y, z = z }
  setmetatable(v, vector)
  return v
end

function vector.__unm(v)
  local result = { x = -v.x, y = -v.y, z = -v.z }
  setmetatable(result, vector)
  return result
end

function vector.__tostring(v)
  return ('%f, %f, %f'):format(v.x, v.y, v.z)
end

setmetatable(vector, {
  __call = function(self, x, y, z)
    x = x or 0
    if type(x) == 'table' then return x end -- Deprecated
    assert(type(x) == 'number', 'vector components must be numbers')
    local instance = { x = x, y = y or x, z = z or (y and 0 or x) }
    setmetatable(instance, self)
    return instance
  end
})

vector.zero = vector(0, 0, 0)
vector.one = vector(1, 1, 1)
vector.left = vector(-1, 0, 0)
vector.right = vector(1, 0, 0)
vector.up = vector(0, 1, 0)
vector.down = vector(0, -1, 0)
vector.forward = vector(0, 0, -1)
vector.backward = vector(0, 0, 1)
vector.back = vector(0, 0, 1)

---

quaternion = {}
quaternion.__index = quaternion

function quaternion.pack(x, y, z, w)
  local result = { x = x, y = y, z = z, w = w }
  setmetatable(result, quaternion)
  return result
end

function quaternion.unpack(q)
  return q.x, q.y, q.z, q.w
end

function quaternion.conjugate(q)
  local result = { x = -q.x, y = -q.y, z = -q.z, w = q.w }
  setmetatable(result, quaternion)
  return result
end

function quaternion.angleaxis(angle, ax, ay, az)
  assert(type(angle) == 'number', 'quaternion angle must be a number')
  assert(type(ax) == 'number' and type(ay) == 'number' and type(az) == 'number', 'quaternion axis components must be numbers')

  local s = sin(angle * .5)
  local c = cos(angle * .5)

  local length = sqrt(ax * ax + ay * ay + az * az)

  local result
  if length > 0 then
    s = s / length
    result = { x = ax * s, y = ay * s, z = az * s, w = c }
  else
    result = { x = 0., y = 0., z = 0., w = 1. }
  end

  setmetatable(result, quaternion)
  return result
end

function quaternion.toangleaxis(q)
  local s = sqrt(1 - q.w * q.w)
  s = s < 1e-6 and 1 or 1 / s

  return 2 * acos(q.w), q.x * s, q.y * s, q.z * s
end

function quaternion.euler(x, y, z)
  local cx, sx = cos(x * .5), sin(x * .5)
  local cy, sy = cos(y * .5), sin(y * .5)
  local cz, sz = cos(z * .5), sin(z * .5)

  local result = {
    x = cy * sx * cz + sy * cx * sz,
    y = sy * cx * cz - cy * sx * sz,
    z = cy * cx * sz - sy * sx * cz,
    w = cy * cx * cz + sy * sx * sz
  }

  setmetatable(result, quaternion)
  return result
end

function quaternion.toeuler(q)
  local x, y, z, w = q.x, q.y, q.z, q.w
  local unit = x * x + y * y + z * z + w * w
  local test = x * w - y * z
  local ax, ay, az

  if test > (.5 - 1e-6) * unit then
    ax = math.pi / 2
    ay = 2 * atan2(y, x)
    az = 0
  elseif test < -(.5 - 1e-6) * unit then
    ax = -math.pi / 2
    ay = -2 * atan2(y, x)
    az = 0
  else
    ax = asin(2 * (w * x - y * z))
    ay = atan2(2 * w * y + 2 * z * x, 1 - 2 * (x * x + y * y))
    az = atan2(2 * w * z + 2 * x * y, 1 - 2 * (z * z + x * x))
  end

  return ax, ay, az
end

function quaternion.between(a, b)
  local dot = a.x * b.x + a.y * b.y + a.z * b.z

  if dot > .99999 or dot < -.99999 then
    return quaternion.identity
  end

  local x = a.y * b.z - a.z * b.y
  local y = a.z * b.x - a.x * b.z
  local z = a.x * b.y - a.y * b.x
  local w = 1 + dot

  local length = sqrt(x * x + y * y + z * z + w * w)

  local result = { x = x / length, y = y / length, z = z / length, w = w / length }
  setmetatable(result, quaternion)
  return result
end

function quaternion.lookdir(dir, up)
  up = up or vector.up

  local fx, fy, fz = -dir.x, -dir.y, -dir.z
  local length = sqrt(fx * fx + fy * fy + fz * fz)

  if length == 0 then
    return quaternion.identity
  end

  fx, fy, fz = fx / length, fy / length, fz / length

  local rx, ry, rz = up.y * fz - up.z * fy, up.z * fx - up.x * fz, up.x * fy - up.y * fx
  length = sqrt(rx * rx + ry * ry + rz * rz)

  if length == 0 then
    if abs(fx) < .9 then
      rx, ry, rz = 0, fz, -fy
    else
      rx, ry, rz = fz, 0, -fx
    end

    length = sqrt(rx * rx + ry * ry + rz * rz)
  end

  rx, ry, rz = rx / length, ry / length, rz / length

  local ux, uy, uz = fy * rz - fz * ry, fz * rx - fx * rz, fx * ry - fy * rx

  local m00, m01, m02 = rx, ry, rz
  local m10, m11, m12 = ux, uy, uz
  local m20, m21, m22 = fx, fy, fz

  local x, y, z, w

  if m22 < 0 then
    if m00 > m11 then
      local t = 1 + m00 - m11 - m22
      local s = .5 / sqrt(t)
      x = t * s
      y = (m01 + m10) * s
      z = (m20 + m02) * s
      w = (m12 - m21) * s
    else
      local t = 1 - m00 + m11 - m22
      local s = .5 / sqrt(t)
      x = (m01 + m10) * s
      y = t * s
      z = (m12 + m21) * s
      w = (m20 - m02) * s
    end
  else
    if m00 < -m11 then
      local t = 1 - m00 - m11 + m22
      local s = .5 / sqrt(t)
      x = (m20 + m02) * s
      y = (m12 + m21) * s
      z = t * s
      w = (m01 - m10) * s
    else
      local t = 1 + m00 + m11 + m22
      local s = .5 / sqrt(t)
      x = (m12 - m21) * s
      y = (m20 - m02) * s
      z = (m01 - m10) * s
      w = t * s
    end
  end

  local result = { x = x, y = y, z = z, w = w }
  setmetatable(result, quaternion)
  return result
end

function quaternion.direction(q)
  local x = -2 * q.x * q.z - 2 * q.w * q.y
  local y = -2 * q.y * q.z + 2 * q.w * q.x
  local z = -1 + 2 * q.x * q.x + 2 * q.y * q.y
  return vector(x, y, z)
end

function quaternion.slerp(q, r, t)
  local dot = q.x * r.x + q.y * r.y + q.z * r.z + q.w * r.w

  if abs(dot) >= 1 then
    return q
  end

  local x, y, z, w = q.x, q.y, q.z, q.w

  if dot < 0 then
    x, y, z, w, dot = -x, -y, -z, -w, -dot
  end

  local halfTheta = acos(dot)
  local sinHalfTheta = sqrt(1 - dot * dot)
  local s = 1 - t

  if abs(sinHalfTheta) < .05 then
    x = x * s + r.x * t
    y = y * s + r.y * t
    z = z * s + r.z * t
    w = w * s + r.w * t

    local length = sqrt(x * x + y * y + z * z + w * w)

    local result = {
      x = x / length,
      y = y / length,
      z = z / length,
      w = w / length
    }

    setmetatable(result, quaternion)
    return result
  end

  local a = sin(s * halfTheta) / sinHalfTheta
  local b = sin(t * halfTheta) / sinHalfTheta

  local result = {
    x = x * a + r.x * b,
    y = y * a + r.y * b,
    z = z * a + r.z * b,
    w = w * a + r.w * b
  }

  setmetatable(result, quaternion)
  return result
end

function quaternion.__mul(q, b)
  if b.w then
    local result = {
      x = q.x * b.w + q.w * b.x + q.y * b.z - q.z * b.y,
      y = q.y * b.w + q.w * b.y + q.z * b.x - q.x * b.z,
      z = q.z * b.w + q.w * b.z + q.x * b.y - q.y * b.x,
      w = q.w * b.w - q.x * b.x - q.y * b.y - q.z * b.z
    }

    setmetatable(result, quaternion)
    return result
  else
    local ux, uy, uz = q.x, q.y, q.z
    local cx, cy, cz = q.y * b.z - q.z * b.y, q.z * b.x - q.x * b.z, q.x * b.y - q.y * b.x

    local uu = ux * ux + uy * uy + uz * uz
    local uv = ux * b.x + uy * b.y + uz * b.z
    local s = q.w * q.w - uu

    local result = {
      x = b.x * s + ux * 2 * uv + cx * 2 * q.w,
      y = b.y * s + uy * 2 * uv + cy * 2 * q.w,
      z = b.z * s + uz * 2 * uv + cz * 2 * q.w
    }

    setmetatable(result, vector)
    return result
  end
end

function quaternion.__tostring(q)
  return ('%f, %f, %f, %f'):format(q.x, q.y, q.z, q.w)
end

setmetatable(quaternion, {
  __call = function(self, ...)
    if ... then
      if type(...) == 'table' then -- Deprecated
        return ...
      else
        return quaternion.angleaxis(...)
      end
    else
      return quaternion.pack(0, 0, 0, 1)
    end
  end
})

quaternion.identity = quaternion.pack(0, 0, 0, 1)
