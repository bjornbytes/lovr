local ffi = type(jit) == 'table' and jit.status() and require 'ffi'
if not ffi then return end

local bit = require 'bit'
local lovr_math, pool = ...
local C = ffi.C

ffi.cdef [[
typedef enum {
  NONE,
  VEC2,
  VEC3,
  VEC4,
  QUAT,
  MAT4
} VectorType;

typedef struct { uint8_t type; uint8_t generation; uint16_t index; } vec2;
typedef struct { uint8_t type; uint8_t generation; uint16_t index; } vec3;
typedef struct { uint8_t type; uint8_t generation; uint16_t index; } vec4;
typedef struct { uint8_t type; uint8_t generation; uint16_t index; } quat;
typedef struct { uint8_t type; uint8_t generation; uint16_t index; } mat4;

typedef struct {
  void* data;
  float* floats;
  size_t count;
  size_t cursor;
  size_t generation;
} Pool;

void lovrPoolGrow(Pool* pool, size_t count);

float* quat_fromMat4(float* q, float* m);
float* quat_between(float* q, float* v, float* u);
void quat_rotate(float* q, float* v);
float* mat4_rotate(float* m, float angle, float ax, float ay, float az);
float* mat4_invert(float* m);
float* mat4_rotateQuat(float* m, float* q);
float* mat4_multiply(float* m, float* n);
]]

pool = ffi.cast('Pool*', pool)

local function allocate(ctype, t, count)
  if pool.cursor + count > pool.count - 4 then
    C.lovrPoolGrow(pool, pool.count * 2)
  end

  local handle = ffi.new(ctype, t, pool.generation, pool.cursor)
  pool.cursor = pool.cursor + count
  return handle
end

local function istype(t, name)
  return function(value)
    return (type(value) == 'cdata' and value.type == t) or (type(value) == 'userdata' and value.__name == name)
  end
end

local function checktype(t, name, ctype)
  return function(value, index, message)
    if type(value) == 'cdata' and value.type == t then
      assert(value.generation == pool.generation, 'wrong generation')
      return pool.floats + value.index
    elseif type(value) == 'userdata' and value.__name == name then
      return ffi.cast('float*', value:getPointer())
    else
      local fn = debug.getinfo(2, 'n').name
      local arg = index and ('argument #' .. index) or 'self'
      local exp = message or name
      local got = type(value) == 'userdata' and value.__name or type(value)
      error(string.format('Bad %s to %s (%s expected, got %s)', arg, fn, exp, got), 3)
    end
  end
end

local vec2_t = ffi.typeof('vec2')
local vec3_t = ffi.typeof('vec3')
local vec4_t = ffi.typeof('vec4')
local quat_t = ffi.typeof('quat')
local mat4_t = ffi.typeof('mat4')
local checkvec2 = checktype(C.VEC2, 'vec2')
local checkvec3 = checktype(C.VEC3, 'vec3')
local checkvec4 = checktype(C.VEC4, 'vec4')
local checkquat = checktype(C.QUAT, 'quat')
local checkmat4 = checktype(C.MAT4, 'mat4')
local isvec2 = istype(C.VEC2, 'vec2')
local isvec3 = istype(C.VEC3, 'vec3')
local isvec4 = istype(C.VEC4, 'vec4')
local isquat = istype(C.QUAT, 'quat')
local ismat4 = istype(C.MAT4, 'mat4')

local vec2, vec3, vec4, quat, mat4

vec2 = {
  unpack = function(self)
    local v = checkvec2(self)
    return v[0], v[1]
  end,

  set = function(self, x, y)
    local v = checkvec2(self)
    if x == nil then
      v[0], v[1] = 0, 0
    elseif type(x) == 'number' then
      v[0], v[1] = x, y or x
    else
      local u = checkvec2(x, 1, 'vec2 or number')
      v[0], v[1] = u[0], u[1]
    end
    return self
  end,

  add = function(self, x)
    local v = checkvec2(self)
    if type(x) == 'number' then
      v[0] = v[0] + x
      v[1] = v[1] + x
    else
      local u = checkvec2(x, 1, 'vec2 or number')
      v[0] = v[0] + u[0]
      v[1] = v[1] + u[1]
    end
    return self
  end,

  sub = function(self, x)
    local v = checkvec2(self)
    if type(x) == 'number' then
      v[0] = v[0] - x
      v[1] = v[1] - x
    else
      local u = checkvec2(x, 1, 'vec2 or number')
      v[0] = v[0] - u[0]
      v[1] = v[1] - u[1]
    end
    return self
  end,

  mul = function(self, x)
    local v = checkvec2(self)
    if type(x) == 'number' then
      v[0] = v[0] * x
      v[1] = v[1] * x
    else
      local u = checkvec2(x, 1, 'vec2 or number')
      v[0] = v[0] * u[0]
      v[1] = v[1] * u[1]
    end
    return self
  end,

  div = function(self, x)
    local v = checkvec2(self)
    if type(x) == 'number' then
      v[0] = v[0] / x
      v[1] = v[1] / x
    else
      local u = checkvec2(x, 1, 'vec2 or number')
      v[0] = v[0] / u[0]
      v[1] = v[1] / u[1]
    end
    return self
  end,

  length = function(self, x)
    local v = checkvec2(self)
    return math.sqrt(v[0] * v[0] + v[1] * v[1])
  end,

  normalize = function(self)
    local v = checkvec2(self)
    local length2 = v[0] * v[0] + v[1] * v[1]
    if length2 ~= 0 then
      local n = 1 / math.sqrt(length2)
      v[0] = v[0] * n
      v[1] = v[1] * n
    end
    return self
  end,

  distance = function(self, other)
    local v = checkvec2(self)
    local u = checkvec2(other)
    local dx = v[0] - u[0]
    local dy = v[1] - u[1]
    return math.sqrt(dx * dx + dy * dy)
  end,

  dot = function(self, other)
    local v = checkvec2(self)
    local u = checkvec2(other, 1, 'vec2')
    return v[0] * u[0] + v[1] * u[1]
  end,

  lerp = function(self, other, t)
    local v = checkvec2(self)
    local u = checkvec2(other, 1, 'vec2')
    v[0] = v[0] + (u[0] - v[0]) * t
    v[1] = v[1] + (u[1] - v[1]) * t
    return self
  end,

  __add = function(a, b)
    checkvec2(a, 1)
    return lovr_math.vec2(a):add(b)
  end,

  __sub = function(a, b)
    checkvec2(a, 1)
    return lovr_math.vec2(a):sub(b)
  end,

  __mul = function(a, b)
    checkvec2(a, 1)
    return lovr_math.vec2(a):mul(b)
  end,

  __div = function(a, b)
    checkvec2(a, 1)
    return lovr_math.vec2(a):div(b)
  end,

  __unm = function(self)
    checkvec2(self)
    return lovr_math.vec2(self):mul(-1)
  end,

  __len = function(self)
    checkvec2(self)
    return self:length()
  end,

  __tostring = function(self)
    local v = checkvec2(self)
    return string.format('(%f, %f)', v[0], v[1])
  end,

  __newindex = function(self, key, value)
    local v = checkvec2(self)

    local swizzles = {
      x = 0,
      y = 1,
      r = 0,
      g = 1,
      s = 0,
      t = 1
    }

    if type(key) == 'string' then
      if #key == 1 then
        local a = swizzles[key]
        if a then
          v[a] = value
          return
        end
      elseif #key == 2 then
        local a, b = swizzles[key:sub(1, 1)], swizzles[key:sub(2, 2)]
        if a and b then
          local u = checkvec2(value, 1)
          v[a], v[b] = u[0], u[1]
          return
        end
      end
    elseif key == 1 or key == 2 then
      v[key - 1] = value
      return
    end

    error(string.format('attempt to assign property %q of vec2 (invalid property)', key), 2)
  end,

  __index = function(self, key)
    if vec2[key] then
      return vec2[key]
    end

    local v = checkvec2(self)

    if type(key) == 'string' then
      local swizzles = {
        x = 0,
        y = 1,
        r = 0,
        g = 1,
        s = 0,
        t = 1
      }

      if #key == 1 then
        local a = swizzles[key]
        if a then
          return v[swizzles[key]]
        end
      elseif #key == 2 then
        local a, b = swizzles[key:sub(1, 1)], swizzles[key:sub(2, 2)]
        if a and b then
          return lovr_math.vec2(v[a], v[b])
        end
      elseif #key == 3 then
        local a, b, c = swizzles[key:sub(1, 1)], swizzles[key:sub(2, 2)], swizzles[key:sub(3, 3)]
        if a and b and c then
          return lovr_math.vec3(v[a], v[b], v[c])
        end
      elseif #key == 4 then
        local a, b, c, d = swizzles[key:sub(1, 1)], swizzles[key:sub(2, 2)], swizzles[key:sub(3, 3)], swizzles[key:sub(4, 4)]
        if a and b and c and d then
          return lovr_math.vec4(v[a], v[b], v[c], v[d])
        end
      end
    elseif key == 1 or key == 2 then
      return v[key - 1]
    end

    error(string.format('attempt to index field %q of vec2 (invalid property)', key), 2)
  end
}

vec3 = {
  unpack = function(self)
    local v = checkvec3(self)
    return v[0], v[1], v[2]
  end,

  set = function(self, x, y, z)
    local v = checkvec3(self)
    if x == nil then
      v[0], v[1], v[2] = 0, 0, 0
    elseif type(x) == 'number' then
      v[0], v[1], v[2] = x, y or x, z or x
    else
      local u = checkvec3(x, 1, 'vec3 or number')
      v[0], v[1], v[2] = u[0], u[1], u[2]
    end
    return self
  end,

  add = function(self, x)
    local v = checkvec3(self)
    if type(x) == 'number' then
      v[0] = v[0] + x
      v[1] = v[1] + x
      v[2] = v[2] + x
    else
      local u = checkvec3(x, 1, 'vec3 or number')
      v[0] = v[0] + u[0]
      v[1] = v[1] + u[1]
      v[2] = v[2] + u[2]
    end
    return self
  end,

  sub = function(self, x)
    local v = checkvec3(self)
    if type(x) == 'number' then
      v[0] = v[0] - x
      v[1] = v[1] - x
      v[2] = v[2] - x
    else
      local u = checkvec3(x, 1, 'vec3 or number')
      v[0] = v[0] - u[0]
      v[1] = v[1] - u[1]
      v[2] = v[2] - u[2]
    end
    return self
  end,

  mul = function(self, x)
    local v = checkvec3(self)
    if type(x) == 'number' then
      v[0] = v[0] * x
      v[1] = v[1] * x
      v[2] = v[2] * x
    else
      local u = checkvec3(x, 1, 'vec3 or number')
      v[0] = v[0] * u[0]
      v[1] = v[1] * u[1]
      v[2] = v[2] * u[2]
    end
    return self
  end,

  div = function(self, x)
    local v = checkvec3(self)
    if type(x) == 'number' then
      v[0] = v[0] / x
      v[1] = v[1] / x
      v[2] = v[2] / x
    else
      local u = checkvec3(x, 1, 'vec3 or number')
      v[0] = v[0] / u[0]
      v[1] = v[1] / u[1]
      v[2] = v[2] / u[2]
    end
    return self
  end,

  length = function(self)
    local v = checkvec3(self)
    return math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
  end,

  normalize = function(self)
    local v = checkvec3(self)
    local length2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2]
    if length2 ~= 0 then
      local n = 1 / math.sqrt(length2)
      v[0] = v[0] * n
      v[1] = v[1] * n
      v[2] = v[2] * n
    end
    return self
  end,

  distance = function(self, other)
    local v = checkvec3(self)
    local u = checkvec3(other, 1)
    local dx = v[0] - u[0]
    local dy = v[1] - u[1]
    local dz = v[2] - u[2]
    return math.sqrt(dx * dx + dy * dy + dz * dz)
  end,

  dot = function(self, other)
    local v = checkvec3(self)
    local u = checkvec3(other, 1)
    return v[0] * u[0] + v[1] * u[1] + v[2] * u[2]
  end,

  cross = function(self, other)
    local v = checkvec3(self)
    local u = checkvec3(other, 1)
    local cx = v[1] * u[2] - v[2] * u[1]
    local cy = v[2] * u[0] - v[0] * u[2]
    local cz = v[0] * u[1] - v[1] * u[0]
    v[0], v[1], v[2] = cx, cy, cz
    return self
  end,

  lerp = function(self, other, t)
    local v = checkvec3(self)
    local u = checkvec3(other, 1)
    v[0] = v[0] + (u[0] - v[0]) * t
    v[1] = v[1] + (u[1] - v[1]) * t
    v[2] = v[2] + (u[2] - v[2]) * t
    return self
  end,

  __add = function(a, b)
    checkvec3(a, 1)
    return lovr_math.vec3(a):add(b)
  end,

  __sub = function(a, b)
    checkvec3(a, 1)
    return lovr_math.vec3(a):sub(b)
  end,

  __mul = function(a, b)
    checkvec3(a, 1)
    return lovr_math.vec3(a):mul(b)
  end,

  __div = function(a, b)
    checkvec3(a, 1)
    return lovr_math.vec3(a):div(b)
  end,

  __unm = function(self)
    checkvec3(self)
    return lovr_math.vec3(self):mul(-1)
  end,

  __len = function(self)
    checkvec3(self)
    return self:length()
  end,

  __tostring = function(self)
    local v = checkvec3(self)
    return string.format('(%f, %f, %f)', v[0], v[1], v[2])
  end,

  __newindex = function(self, key, value)
    local v = checkvec3(self)

    local swizzles = {
      x = 0,
      y = 1,
      z = 2,
      r = 0,
      g = 1,
      b = 2,
      s = 0,
      t = 1,
      p = 2
    }

    if type(key) == 'string' then
      if #key == 1 then
        local a = swizzles[key:sub(1, 1)]
        if a then
          v[a] = value
          return
        end
      elseif #key == 2 then
        local a, b = swizzles[key:sub(1, 1)], swizzles[key:sub(2, 2)]
        if a and b then
          local u = checkvec2(value, 1)
          v[a], v[b] = u[0], u[1]
          return
        end
      elseif #key == 3 then
        local a, b, c = swizzles[key:sub(1, 1)], swizzles[key:sub(2, 2)], swizzles[key:sub(3, 3)]
        if a and b and c then
          local u = checkvec3(value, 1)
          v[a], v[b], v[c] = u[0], u[1], u[2]
          return
        end
      end
    elseif type(key) == 'number' then
      if key >= 1 and key <= 3 then
        v[key - 1] = value
      end
    end

    error(string.format('attempt to assign property %q of vec3 (invalid property)', key), 2)
  end,

  __index = function(self, key)
    if vec3[key] then
      return vec3[key]
    end

    local v = checkvec3(self)

    if type(key) == 'string' then
      local swizzles = {
        x = 0,
        y = 1,
        z = 2,
        r = 0,
        g = 1,
        b = 2,
        s = 0,
        t = 1,
        p = 2
      }

      if #key == 1 then
        local a = swizzles[key:sub(1, 1)]
        if a then
          return v[a]
        end
      elseif #key == 2 then
        local a, b = swizzles[key:sub(1, 1)], swizzles[key:sub(2, 2)]
        if a and b then
          return lovr_math.vec2(v[a], v[b])
        end
      elseif #key == 3 then
        local a, b, c = swizzles[key:sub(1, 1)], swizzles[key:sub(2, 2)], swizzles[key:sub(3, 3)]
        if a and b and c then
          return lovr_math.vec3(v[a], v[b], v[c])
        end
      elseif #key == 4 then
        local a, b, c, d = swizzles[key:sub(1, 1)], swizzles[key:sub(2, 2)], swizzles[key:sub(3, 3)], swizzles[key:sub(4, 4)]
        if a and b and c and d then
          return lovr_math.vec4(v[a], v[b], v[c], v[d])
        end
      end
    elseif type(key) == 'number' and key >= 1 and key <= 3 then
      return v[key - 1]
    end

    error(string.format('attempt to index field %q of vec3 (invalid property)', type(key)), 2)
  end
}

vec4 = {
  unpack = function(self)
    local v = checkvec4(self)
    return v[0], v[1], v[2], v[3]
  end,

  set = function(self, x, y, z, w)
    local v = checkvec4(self)
    if x == nil then
      v[0], v[1], v[2], v[3] = 0, 0, 0, 0
    elseif type(x) == 'number' then
      v[0], v[1], v[2], v[3] = x, y or x, z or x, w or x
    else
      local u = checkvec4(x, 1, 'vec4 or number')
      v[0], v[1], v[2], v[3] = u[0], u[1], u[2], u[3]
    end
    return self
  end,

  add = function(self, x)
    local v = checkvec3(self)
    if type(x) == 'number' then
      v[0] = v[0] + x
      v[1] = v[1] + x
      v[2] = v[2] + x
      v[3] = v[3] + x
    else
      local u = checkvec4(x, 1, 'vec4 or number')
      v[0] = v[0] + u[0]
      v[1] = v[1] + u[1]
      v[2] = v[2] + u[2]
      v[3] = v[3] + u[3]
    end
    return self
  end,

  sub = function(self, x)
    local v = checkvec4(self)
    if type(x) == 'number' then
      v[0] = v[0] - x
      v[1] = v[1] - x
      v[2] = v[2] - x
      v[3] = v[3] - x
    else
      local u = checkvec4(x, 1, 'vec4 or number')
      v[0] = v[0] - u[0]
      v[1] = v[1] - u[1]
      v[2] = v[2] - u[2]
      v[3] = v[3] - u[3]
    end
    return self
  end,

  mul = function(self, x)
    local v = checkvec4(self)
    if type(x) == 'number' then
      v[0] = v[0] * x
      v[1] = v[1] * x
      v[2] = v[2] * x
      v[3] = v[3] * x
    else
      local u = checkvec4(x, 1, 'vec4 or number')
      v[0] = v[0] * u[0]
      v[1] = v[1] * u[1]
      v[2] = v[2] * u[2]
      v[3] = v[3] * u[3]
    end
    return self
  end,

  div = function(self, x)
    local v = checkvec4(self)
    if type(x) == 'number' then
      v[0] = v[0] / x
      v[1] = v[1] / x
      v[2] = v[2] / x
      v[3] = v[3] / x
    else
      local u = checkvec4(x, 1, 'vec4 or number')
      v[0] = v[0] / u[0]
      v[1] = v[1] / u[1]
      v[2] = v[2] / u[2]
      v[3] = v[3] / u[3]
    end
    return self
  end,

  length = function(self)
    local v = checkvec4(self)
    return math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3])
  end,

  normalize = function(self)
    local v = checkvec4(self)
    local length2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3]
    if length2 ~= 0 then
      local n = 1 / math.sqrt(length2)
      v[0] = v[0] * n
      v[1] = v[1] * n
      v[2] = v[2] * n
      v[3] = v[3] * n
    end
    return self
  end,

  distance = function(self, other)
    local v = checkvec4(self)
    local u = checkvec4(other, 1)
    local dx = v[0] - u[0]
    local dy = v[1] - u[1]
    local dz = v[2] - u[2]
    local dw = v[3] - u[3]
    return math.sqrt(dx * dx + dy * dy + dz * dz + dw * dw)
  end,

  dot = function(self, other)
    local v = checkvec4(self)
    local u = checkvec4(other, 1)
    return v[0] * u[0] + v[1] * u[1] + v[2] * u[2] + u[3] * v[3]
  end,

  lerp = function(self, other, t)
    local v = checkvec4(self)
    local u = checkvec4(other, 1)
    v[0] = v[0] + (u[0] - v[0]) * t
    v[1] = v[1] + (u[1] - v[1]) * t
    v[2] = v[2] + (u[2] - v[2]) * t
    v[3] = v[3] + (u[3] - v[3]) * t
    return self
  end,

  __add = function(a, b)
    checkvec4(a, 1)
    return lovr_math.vec4(a):add(b)
  end,

  __sub = function(a, b)
    checkvec4(a, 1)
    return lovr_math.vec4(a):sub(b)
  end,

  __mul = function(a, b)
    checkvec4(a, 1)
    return lovr_math.vec4(a):mul(b)
  end,

  __div = function(a, b)
    checkvec4(a, 1)
    return lovr_math.vec4(a):div(b)
  end,

  __unm = function(self)
    checkvec4(self)
    return lovr_math.vec4(self):mul(-1)
  end,

  __len = function(self)
    checkvec4(self)
    return self:length()
  end,

  __tostring = function(self)
    local v = checkvec4(self)
    return string.format('(%f, %f, %f, %f)', v[0], v[1], v[2], v[3])
  end,

  __newindex = function(self, key, value)
    local v = checkvec3(self)

    local swizzles = {
      x = 0,
      y = 1,
      z = 2,
      w = 3,
      r = 0,
      g = 1,
      b = 2,
      a = 3,
      s = 0,
      t = 1,
      p = 2,
      q = 3
    }

    if type(key) == 'string' then
      if #key == 1 then
        local a = swizzles[key:sub(1, 1)]
        if a then
          v[a] = value
          return
        end
      elseif #key == 2 then
        local a, b = swizzles[key:sub(1, 1)], swizzles[key:sub(2, 2)]
        if a and b then
          local u = checkvec2(value, 1)
          v[a], v[b] = u[0], u[1]
          return
        end
      elseif #key == 3 then
        local a, b, c = swizzles[key:sub(1, 1)], swizzles[key:sub(2, 2)], swizzles[key:sub(3, 3)]
        if a and b and c then
          local u = checkvec3(value, 1)
          v[a], v[b], v[c] = u[0], u[1], u[2]
          return
        end
      elseif #key == 4 then
        local a, b, c, d = swizzles[key:sub(1, 1)], swizzles[key:sub(2, 2)], swizzles[key:sub(3, 3)], swizzles[key:sub(4, 4)]
        if a and b and c and d then
          local u = checkvec4(value, 1)
          v[a], v[b], v[c], v[d] = u[0], u[1], u[2], u[3]
          return
        end
      end
    elseif type(key) == 'number' then
      if key >= 1 and key <= 4 then
        v[key - 1] = value
      end
    end

    error(string.format('attempt to assign property %q of vec4 (invalid property)', key), 2)
  end,

  __index = function(self, key)
    if vec4[key] then
      return vec4[key]
    end

    local v = checkvec4(self)

    if type(key) == 'string' then
      local swizzles = {
        x = 0,
        y = 1,
        z = 2,
        w = 3,
        r = 0,
        g = 1,
        b = 2,
        a = 3,
        s = 0,
        t = 1,
        p = 2,
        q = 3
      }

      if #key == 1 then
        local a = swizzles[key:sub(1, 1)]
        if a then
          return v[a]
        end
      elseif #key == 2 then
        local a, b = swizzles[key:sub(1, 1)], swizzles[key:sub(2, 2)]
        if a and b then
          return lovr_math.vec2(v[a], v[b])
        end
      elseif #key == 3 then
        local a, b, c = swizzles[key:sub(1, 1)], swizzles[key:sub(2, 2)], swizzles[key:sub(3, 3)]
        if a and b and c then
          return lovr_math.vec3(v[a], v[b], v[c])
        end
      elseif #key == 4 then
        local a, b, c, d = swizzles[key:sub(1, 1)], swizzles[key:sub(2, 2)], swizzles[key:sub(3, 3)], swizzles[key:sub(4, 4)]
        if a and b and c and d then
          return lovr_math.vec4(v[a], v[b], v[c], v[d])
        end
      end
    elseif type(key) == 'number' and key >= 1 and key <= 4 then
      return v[key - 1]
    end

    error(string.format('attempt to index field %q of vec4 (invalid property)', type(key)), 2)
  end
}


quat = {
  unpack = function(self, raw)
    local q = checkquat(self)
    if raw then
      return q[0], q[1], q[2], q[3]
    else
      local s = math.sqrt(1 - q[3] * q[3])
      s = s < .0001 and 1 or 1 / s
      return 2 * math.acos(q[3]), q[0] * s, q[1] * s, q[2] * s
    end
  end,

  set = function(self, x, y, z, w, raw)
    local q = checkquat(self)
    if x == nil then
      q[0], q[1], q[2], q[3] = 0, 0, 0, 1
    elseif type(x) == 'number' then
      if raw then
        q[0], q[1], q[2], q[3] = x, y, z, w
      else
        local s = math.sin(x * .5)
        local c = math.cos(x * .5)
        local length = y * y + z * z + w * w
        if length > 0 then
          s = s / math.sqrt(length)
        end
        q[0], q[1], q[2], q[3] = s * y, s * z, s * w, c
      end
    elseif isvec3(x) then
      local v = checkvec3(x, 1, 'nil, number, vec3, quat, or mat4')
      if y == nil then
        C.quat_between(lovr_math.vec3(0, 0, -1), v)
      else
        local u = checkvec3(y, 2, 'nil or vec3')
        C.quat_between(q, v, u) -- inline
      end
    elseif isquat(x) then
      local r = checkquat(x, 1, 'nil, number, vec3, quat, or mat4')
      q[0], q[1], q[2], q[3] = r[0], r[1], r[2], r[3]
    else
      local m = checkmat4(x, 1, 'nil, number, vec3, quat, or mat4')
      C.quat_fromMat4(q, m)
    end
    return self
  end,

  mul = function(self, other)
    local q = checkquat(self)
    if isquat(other) then
      local r = checkquat(other, 1, 'quat or vec3')
      local x = q[0] * r[3] + q[3] * r[0] + q[1] * r[2] - q[2] * r[1]
      local y = q[1] * r[3] + q[3] * r[1] + q[2] * r[0] - q[0] * r[2]
      local z = q[2] * r[3] + q[3] * r[2] + q[0] * r[1] - q[1] * r[0]
      local w = q[3] * r[3] - q[0] * r[0] - q[1] * r[1] - q[2] * r[2]
      q[0], q[1], q[2], q[3] = x, y, z, w
      return self
    else
      local v = checkvec3(other, 1, 'quat or vec3')
      C.quat_rotate(q, v)
      return other
    end
  end,

  length = function(self)
    local q = checkquat(self)
    return math.sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3])
  end,

  normalize = function(self)
    local q = checkquat(self)
    local length2 = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]
    if length ~= 0 then
      local n = 1 / math.sqrt(length2)
      q[0] = q[0] * n
      q[1] = q[1] * n
      q[2] = q[2] * n
      q[3] = q[3] * n
    end
    return self
  end,

  direction = function(self)
    local q = checkquat(self)
    local dx = 2 * q[0] * q[2] + 2 * q[3] * q[1]
    local dy = 2 * q[1] * q[2] - 2 * q[3] * q[0]
    local dz = 1 - 2 * q[0] * q[0] - 2 * q[1] * q[1]
    return -dx, -dy, -dz
  end,

  conjugate = function(self)
    local q = checkquat(self)
    q[0] = -q[0]
    q[1] = -q[1]
    q[2] = -q[2]
    return self
  end,

  slerp = function(self, other, t)
    local q = checkquat(self)
    local r = checkquat(other, 1)

    local dot = q[0] * r[0] + q[1] * r[1] + q[2] * r[2] + q[3] * r[3]

    if math.abs(dot) >= 1 then
      return self
    elseif dot < 0 then
      q[0] = -q[0]
      q[1] = -q[1]
      q[2] = -q[2]
      q[3] = -q[3]
      dot = -dot
    end

    local halfTheta = math.acos(dot)
    local sinHalfTheta = math.sin(1 - dot * dot)

    if math.abs(sinHalfTheta) < .001 then
      q[0] = q[0] * .5 + r[0] * .5
      q[1] = q[1] * .5 + r[1] * .5
      q[2] = q[2] * .5 + r[2] * .5
      q[3] = q[3] * .5 + r[3] * .5
      return self
    end

    local a = math.sin((1 - t) * halfTheta) / sinHalfTheta
    local b = math.sin(t * halfTheta) / sinHalfTheta
    q[0] = q[0] * a + r[0] * b
    q[1] = q[1] * a + r[1] * b
    q[2] = q[2] * a + r[2] * b
    q[3] = q[3] * a + r[3] * b
    return self
  end,

  __mul = function(a, b)
    checkquat(a, 1)
    if isquat(b) then
      return lovr_math.quat(a):mul(b)
    elseif isvec3(b) then
      return a:mul(lovr_math.vec3(b))
    end
    error(string.format('attempt to perform arithmetic on quat and %s (unsupported)', type(b)), 2)
  end,

  __len = function(self)
    return self:length()
  end,

  __tostring = function(self)
    local q = checkquat(self)
    return string.format('(%f, %f, %f, %f)', q[0], q[1], q[2], q[3])
  end,

  __newindex = function(self, key, value)
    local q = checkquat(self)

    if key == 'x' or key == 1 then
      q[0] = value
    elseif key == 'y' or key == 2 then
      q[1] = value
    elseif key == 'z' or key == 3 then
      q[2] = value
    elseif key == 'w' or key == 4 then
      q[3] = value
    end

    error(string.format('attempt to assign property %q of quat (invalid property)', key), 2)
  end,

  __index = function(self, key)
    if quat[key] then
      return quat[key]
    end

    local q = checkquat(self)

    if key == 'x' or key == 1 then
      return q[0]
    elseif key == 'y' or key == 2 then
      return q[1]
    elseif key == 'z' or key == 3 then
      return q[2]
    elseif key == 'w' or key == 4 then
      return q[3]
    end

    error(string.format('attempt to index field %q of quat (invalid property)', key), 2)
  end
}

mat4 = {
  unpack = function(self, raw)
    local m = checkmat4(self)
    if raw then
      return m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]
    else
      local x, y, z = m[12], m[13], m[14]
      local sx = math.sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2])
      local sy = math.sqrt(m[4] * m[4] + m[5] * m[5] + m[6] * m[6])
      local sz = math.sqrt(m[8] * m[8] + m[9] * m[9] + m[10] * m[10])
      local angle = math.acos((m[0] + m[5] + m[10] - 1) / 2)
      local ax, ay, az = m[6] - m[9], m[8] - m[2], m[1] - m[4]
      local length = math.sqrt(ax * ax + ay * ay + az * az)
      if length > 0 then
        ax = ax / length
        ay = ay / length
        az = az / length
      end
      return x, y, z, sx, sy, sz, angle, ax, ay, az
    end
  end,

  set = function(self, ...)
    local m = checkmat4(self)
    local count = select('#', ...)

    if ... == nil then
      self:identity()
    elseif count == 1 and type(...) == 'number' then
      local x = ...
      m[0],  m[1],  m[2],  m[3]  = x, 0, 0, 0
      m[4],  m[5],  m[6],  m[7]  = 0, x, 0, 0
      m[8],  m[9],  m[10], m[11] = 0, 0, x, 0
      m[12], m[13], m[14], m[15] = 0, 0, 0, x
    elseif count == 16 then
      m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15] = ...
    elseif ismat4(...) then
      local n = checkmat4(..., 1)
      ffi.copy(m, n, 64)
    else
      local i = 1
      self:identity()

      -- Position
      local x, y, z = ...
      if isvec3(x) then
        local v = checkvec3(x, 1)
        m[12], m[13], m[14] = v[0], v[1], v[2]
        i = i + 1
      else
        m[12], m[13], m[14] = x, y, z
        i = i + 3
      end

      -- Scale / Rotation
      local x, y, z, w = select(i, ...)
      if isquat(x) then
        local q = checkquat(x, i)
        C.mat4_rotateQuat(m, q)
        return self
      -- If there are exactly 4 arguments and the last one is a number, assume [angle, ax, ay, az] (YIKES)
      elseif (count - i) == 3 and type(w) == 'number' then
        C.mat4_rotate(m, x, y, z, w)
        return self
      elseif isvec3(x) then
        local v = checkvec3(x, 1)
        m[0], m[5], m[10] = v[0], v[1], v[2]
        i = i + 1
      else
        m[0], m[5], m[10] = x, y, z
        i = i + 3
      end

      -- Rotation (if scaled)
      local x, y, z, w = select(i, ...)
      if isquat(x) then
        local q = checkquat(x, i)
        C.mat4_rotateQuat(m, q)
      else
        C.mat4_rotate(m, x, y, z, w)
      end
    end

    return self
  end,

  mul = function(self, other)
    local m = checkmat4(self)
    if ismat4(other) then
      local n = checkmat4(other, 1)
      C.mat4_multiply(m, n)
    elseif isvec3(other) then
      local v = checkvec3(other, 1)
      local x = v[0] * m[0] + v[1] * m[4] + v[2] * m[8] + m[12]
      local y = v[0] * m[1] + v[1] * m[5] + v[2] * m[9] + m[13]
      local z = v[0] * m[2] + v[1] * m[6] + v[2] * m[10] + m[14]
      v[0], v[1], v[2] = x, y, z
      return other
    end
    return self
  end,

  identity = function(self)
    local m = checkmat4(self)
    m[0],  m[1],  m[2],  m[3]  = 1, 0, 0, 0
    m[4],  m[5],  m[6],  m[7]  = 0, 1, 0, 0
    m[8],  m[9],  m[10], m[11] = 0, 0, 1, 0
    m[12], m[13], m[14], m[15] = 0, 0, 0, 1
    return self
  end,

  invert = function(self)
    local m = checkmat4(self)
    C.mat4_invert(m)
    return self
  end,

  transpose = function(self)
    local m = checkmat4(self)
    local m1, m2, m3, m6, m7, m11 = m[1], m[2], m[3], m[6], m[7], m[11]
    m[1] = m[4]
    m[2] = m[8]
    m[3] = m[12]
    m[4] = m1
    m[6] = m[9]
    m[7] = m[13]
    m[8] = m2
    m[9] = m6
    m[11] = m[14]
    m[12] = m3
    m[13] = m7
    m[14] = m11
    return self
  end,

  translate = function(self, x, y, z)
    local m = checkmat4(self)
    if type(x) ~= 'number' then
      checkvec3(x, 1, 'vec3 or number')
      x, y, z = x.x, x.y, x.z
    end
    m[12] = m[0] * x + m[4] * y + m[8] * z + m[12]
    m[13] = m[1] * x + m[5] * y + m[9] * z + m[13]
    m[14] = m[2] * x + m[6] * y + m[10] * z + m[14]
    m[15] = m[3] * x + m[7] * y + m[11] * z + m[15]
    return self
  end,

  rotate = function(self, angle, ax, ay, az)
    local m = checkmat4(self)
    if type(angle) == 'number' then
      C.mat4_rotate(m, angle, ax, ay, az)
    else
      checkquat(angle, 1, 'quat or number')
      C.mat4_rotateQuat(m, angle)
    end
    return self
  end,

  scale = function(self, sx, sy, sz)
    local m = checkmat4(self)
    if type(sx) ~= 'number' then
      checkvec3(sx, 1, 'vec3 or number')
      sx, sy, sz = sx.x, sx.y, sx.z
    end
    m[0] = m[0] * sx
    m[1] = m[1] * sx
    m[2] = m[2] * sx
    m[3] = m[3] * sx
    m[4] = m[4] * sy
    m[5] = m[5] * sy
    m[6] = m[6] * sy
    m[7] = m[7] * sy
    m[8] = m[8] * sz
    m[9] = m[9] * sz
    m[10] = m[10] * sz
    m[11] = m[11] * sz
    return self
  end,

  orthographic = function(self, left, right, top, bottom, near, far)
    local m = checkmat4(self)
    local rl = right - left
    local tb = top - bottom
    local fn = far - near
    m[0] = 2 / rl
    m[1] = 0
    m[2] = 0
    m[3] = 0
    m[4] = 0
    m[5] = 2 / tb
    m[6] = 0
    m[7] = 0
    m[8] = 0
    m[9] = 0
    m[10] = -2 / fn
    m[11] = 0
    m[12] = -(left + right) / rl
    m[13] = -(top + bottom) / tb
    m[14] = -(far + near) / fn
    m[15] = 1
    return self
  end,

  perspective = function(self, near, far, fov, aspect)
    local m = checkmat4(self)
    local range = math.tan(fov * .5) * near
    local sx = (2 * near) / (range * aspect + range * aspect)
    local sy = near / range
    local sz = -(far + near) / (far - near)
    local pz = (-2 * far * near) / (far - near)
    m[0] = sx
    m[1] = 0
    m[2] = 0
    m[3] = 0
    m[4] = 0
    m[5] = sy
    m[6] = 0
    m[7] = 0
    m[8] = 0
    m[9] = 0
    m[10] = sz
    m[11] = -1
    m[12] = 0
    m[13] = 0
    m[14] = pz
    m[15] = 0
    return self
  end,

  fov = function(self, left, right, up, down, near, far)
    local m = checkmat4(self)
    local idx = 1 / (right - left)
    local idy = 1 / (down - up)
    local idz = 1 / (far - near)
    local sx = right + left
    local sy = down + up
    m[0] = 2 * idx
    m[1] = 0
    m[2] = 0
    m[3] = 0
    m[4] = 0
    m[5] = 2 * idy
    m[6] = 0
    m[7] = 0
    m[8] = sx * idx
    m[9] = sy * idy
    m[10] = -far * idz
    m[11] = -1
    m[12] = 0
    m[13] = 0
    m[14] = -far * near * idz
    m[15] = 0
    return self
  end,

  lookAt = function(self, from, to, up) -- From excessive/cpml
    local m = checkmat4(self)
    local z = (to - from):normalize()
    local x = lovr_math.vec3(up):cross(z):normalize()
    local y = z:cross(x)
    m[0] = x.x
    m[1] = y.x
    m[2] = z.x
    m[3] = 0
    m[4] = x.y
    m[5] = y.y
    m[6] = z.y
    m[7] = 0
    m[8] = x.z
    m[9] = y.z
    m[10] = z.z
    m[11] = 0
    m[12] = 0
    m[13] = 0
    m[14] = 0
    m[15] = 1
  end,

  __mul = function(self, other)
    local m = checkmat4(self)
    if ismat4(other) then
      return lovr_math.mat4(self):mul(other)
    elseif isvec3(other) then
      return self:mul(lovr_math.vec3(other))
    end
    error(string.format('attempt to perform arithmetic on mat4 and %s (unsupported)', type(other)), 2)
  end,

  __newindex = function(self, key, value)
    local m = checkmat4(self)

    if type(key) == 'number' and key >= 1 and key <= 16 then
      m[key - 1] = value
    end

    error(string.format('attempt to assign to property %q of mat4 (invalid property)', key), 2)
  end,

  __index = function(self, key)
    if mat4[key] then
      return mat4[key]
    end

    local m = checkmat4(self)

    if type(key) == 'number' and key >= 1 and key <= 16 then
      return m[key - 1]
    end

    error(string.format('attempt to index field %q of mat4 (invalid property)', key), 2)
  end
}

ffi.metatype(vec2_t, vec2)
ffi.metatype(vec3_t, vec3)
ffi.metatype(vec4_t, vec4)
ffi.metatype(quat_t, quat)
ffi.metatype(mat4_t, mat4)

local new = ffi.new
lovr_math.vec2 = function(...) return allocate(vec2_t, C.VEC2, 2):set(...) end
lovr_math.vec3 = function(...) return allocate(vec3_t, C.VEC3, 4):set(...) end
lovr_math.vec4 = function(...) return allocate(vec4_t, C.VEC4, 4):set(...) end
lovr_math.quat = function(...) return allocate(quat_t, C.QUAT, 4):set(...) end
lovr_math.mat4 = function(...) return allocate(mat4_t, C.MAT4, 16):set(...) end
lovr_math.drain = function()
  pool.cursor = 0
  pool.generation = bit.band(pool.generation + 1, 255)
end
