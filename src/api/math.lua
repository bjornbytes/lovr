local ffi = type(jit) == 'table' and jit.status() and require 'ffi'
if not ffi then return end

local math, Pool = ...
local C, new, cast, typeof, istype = ffi.C, ffi.new, ffi.cast, ffi.typeof, ffi.istype

ffi.cdef [[
  typedef union {
    struct { float x; float y; float z; };
    float _p[4];
  } vec3;

  typedef union {
    struct { float x; float y; float z; float w; };
    float _p[4];
  } quat;

  typedef union {
    struct { float m[16]; };
    float _p[16];
  } mat4;

  typedef struct Pool Pool;
  enum { MATH_VEC3, MATH_QUAT, MATH_MAT4 };
  vec3* lovrPoolAllocateVec3(Pool* pool);
  quat* lovrPoolAllocateQuat(Pool* pool);
  mat4* lovrPoolAllocateMat4(Pool* pool);

  // Careful, the declarations below are using the structs above, not the usual C types in lib/math.h

  vec3* vec3_normalize(vec3* v);
  float vec3_length(vec3* v);
  float vec3_distance(vec3* v, vec3* u);
  vec3* vec3_cross(vec3* v, vec3* u);
  vec3* vec3_lerp(vec3* v, vec3* u, float t);

  quat* quat_fromAngleAxis(quat* q, float angle, float ax, float ay, float az);
  quat* quat_fromMat4(quat* q, mat4* m);
  quat* quat_mul(quat* q, quat* r);
  quat* quat_normalize(quat* q);
  float quat_length(quat* q);
  quat* quat_slerp(quat* q, quat* r, float t);
  void quat_rotate(quat* q, vec3* v);
  void quat_getAngleAxis(quat* q, float* angle, float* x, float* y, float* z);
  quat* quat_between(quat* q, vec3* u, vec3* v);

  mat4* mat4_set(mat4* m, mat4* n);
  mat4* mat4_identity(mat4* m);
  mat4* mat4_invert(mat4* m);
  mat4* mat4_transpose(mat4* m);
  mat4* mat4_multiply(mat4* m, mat4* n);
  mat4* mat4_translate(mat4* m, float x, float y, float z);
  mat4* mat4_rotate(mat4* m, float angle, float x, float y, float z);
  mat4* mat4_rotateQuat(mat4* m, quat* q);
  mat4* mat4_scale(mat4* m, float x, float y, float z);
  void mat4_getTransform(mat4* m, float* x, float* y, float* z, float* sx, float* sy, float* sz, float* angle, float* ax, float* ay, float* az);
  mat4* mat4_setTransform(mat4* m, float x, float y, float z, float sx, float sy, float sz, float angle, float ax, float ay, float az);
  mat4* mat4_orthographic(mat4* m, float left, float right, float top, float bottom, float near, float far);
  mat4* mat4_perspective(mat4* m, float near, float far, float fov, float aspect);
  void mat4_transform(mat4* m, float* x, float* y, float* z);
]]

local function checktype(t, name)
  return function(x, i, exp)
    if not istype(t, x) then
      local fn, arg, exp = debug.getinfo(2, 'n').name, i and ('argument #' .. i) or 'self', exp or name
      error(string.format('Bad %s to %s (%s expected, got %s)', arg, fn, exp, type(x)), 3)
    end
  end
end

local vec3_t = ffi.typeof('vec3')
local quat_t = ffi.typeof('quat')
local mat4_t = ffi.typeof('mat4')

local checkvec3 = checktype(vec3_t, 'vec3')
local checkquat = checktype(quat_t, 'quat')
local checkmat4 = checktype(mat4_t, 'mat4')

local vec3 = {
  _type = C.MATH_VEC3,

  unpack = function(v)
    checkvec3(v)
    return v.x, v.y, v.z
  end,

  set = function(v, x, y, z)
    checkvec3(v)
    if x == nil or type(x) == 'number' then
      v.x, v.y, v.z = x or 0, y or x or 0, z or x or 0
    else
      checkvec3(x, 1)
      v.x, v.y, v.z = x.x, x.y, x.z
    end
    return v
  end,

  add = function(v, u)
    checkvec3(v)
    checkvec3(u, 1)
    v.x = v.x + u.x
    v.y = v.y + u.y
    v.z = v.z + u.z
    return v
  end,

  sub = function(v, u)
    checkvec3(v)
    checkvec3(u, 1)
    v.x = v.x - u.x
    v.y = v.y - u.y
    v.z = v.z - u.z
    return v
  end,

  mul = function(v, u)
    checkvec3(v)
    if type(u) == 'number' then
      v.x = v.x * u
      v.y = v.y * u
      v.z = v.z * u
      return v
    else
      checkvec3(u, 1)
      v.x = v.x * u.x
      v.y = v.y * u.y
      v.z = v.z * u.z
      return v
    end
  end,

  div = function(v, u)
    checkvec3(v)
    if type(u) == 'number' then
      v.x = v.x / u
      v.y = v.y / u
      v.z = v.z / u
      return v
    else
      checkvec3(u, 1)
      v.x = v.x / u.x
      v.y = v.y / u.y
      v.z = v.z / u.z
      return v
    end
  end,

  length = function(v)
    checkvec3(v)
    return C.vec3_length(v)
  end,

  normalize = function(v)
    checkvec3(v)
    C.vec3_normalize(v)
    return v
  end,

  distance = function(v, u)
    checkvec3(v)
    checkvec3(u, 1)
    return C.vec3_distance(v, u)
  end,

  dot = function(v, u)
    checkvec3(v)
    checkvec3(u, 1)
    return v.x * u.x + v.y * u.y + v.z * u.z
  end,

  cross = function(v, u)
    checkvec3(v)
    checkvec3(u, 1)
    C.vec3_cross(v, u)
    return v
  end,

  lerp = function(v, u, t)
    checkvec3(v)
    checkvec3(u, 1)
    C.vec3_lerp(v, u, t)
    return v
  end,

  __add = function(v, u)
    checkvec3(v, 1)
    checkvec3(u, 2)
    return vec3_t(v.x + u.x, v.y + u.y, v.z + u.z)
  end,

  __sub = function(v, u)
    checkvec3(v, 1)
    checkvec3(u, 2)
    return vec3_t(v.x - u.x, v.y - u.y, v.z - u.z)
  end,

  __mul = function(v, u)
    if type(v) == 'number' then
      checkvec3(u, 2)
      return vec3_t(v * u.x, v * u.y, v * u.z)
    elseif type(u) == 'number' then
      checkvec3(v, 1)
      return vec3_t(v.x * u, v.y * u, v.z * u)
    else
      checkvec3(v, 1)
      checkvec3(u, 2, 'vec3 or number')
      return vec3_t(v.x * u.x, v.y * u.y, v.z * u.z)
    end
  end,

  __div = function(v, u)
    if type(v) == 'number' then
      checkvec3(u, 2)
      return vec3_t(v / u.x, v / u.y, v / u.z)
    elseif type(u) == 'number' then
      checkvec3(v, 1)
      return vec3_t(v.x / u, v.y / u, v.z / u)
    else
      checkvec3(v, 1)
      checkvec3(u, 'vec3 or number')
      return vec3_t(v.x / u.x, v.y / u.y, v.z / u.z)
    end
  end,

  __unm = function(v) return vec3_t(-v.x, -v.y, -v.z) end,
  __len = function(v) return C.vec3_length(v) end,
  __tostring = function(v) return string.format('(%f, %f, %f)', v.x, v.y, v.z) end
}

vec3.__index = vec3
ffi.metatype(vec3_t, vec3)
math.vec3 = setmetatable(vec3, {
  __call = function(_, ...)
    return vec3_t():set(...)
  end
})

local quat = {
  _type = C.MATH_QUAT,

  unpack = function(q, raw)
    checkquat(q)
    if raw then
      return q.x, q.y, q.z, q.w
    else
      local f = new('float[4]')
      C.quat_getAngleAxis(q, f + 0, f + 1, f + 2, f + 3)
      return f[0], f[1], f[2], f[3]
    end
  end,

  set = function(q, x, y, z, w, raw)
    checkquat(q)
    if type(x) == 'number' then
      if type(y) == 'number' then
        if raw then
          q.x, q.y, q.z, q.w = x, y, z, w
        else
          C.quat_fromAngleAxis(q, x, y, z, w)
        end
      else
        local axis = checkvec3(y, 2)
        C.quat_fromAngleAxis(q, x, y.x, y.y, y.z)
      end
    elseif istype(vec3, x) then
      if istype(vec3, y) then
        C.quat_between(q, x, y)
      else
        local forward = new('float[3]', 0, 0, -1)
        C.quat_between(forward, x)
      end
    elseif istype(quat, x) then
      q.x, q.y, q.z, q.w = x.x, x.y, x.z, x.w
    elseif istype(mat4, x) then
      C.quat_fromMat4(q, x)
    end
    return q
  end,

  mul = function(q, r)
    checkquat(q)
    if istype(vec3, r) then
      C.quat_rotate(q, r)
      return r
    else
      checkquat(r, 1, 'vec3 or quat')
      C.quat_mul(q, r)
      return q
    end
  end,

  length = function(q)
    checkquat(q)
    return C.quat_length(q)
  end,

  normalize = function(q)
    checkquat(q)
    return C.quat_normalize(q)
  end,

  slerp = function(q, r, t)
    checkquat(q)
    checkquat(r, 1)
    return C.quat_slerp(q, r, t)
  end,

  __mul = function(q, r)
    checkquat(q, 1)
    if istype(vec3, r) then
      local v = math.vec3(r)
      C.quat_rotate(q, v)
      return v
    else
      checkquat(r, 2, 'vec3 or quat')
      return C.quat_mul(quat_t(q), r)
    end
  end,

  __len = function(q)
    checkquat(q)
    return C.quat_length(q)
  end,

  __tostring = function(q) return string.format('(%f, %f, %f, %f)', q.x, q.y, q.z, q.w) end
}

quat.__index = quat
ffi.metatype(quat_t, quat)
math.quat = setmetatable(quat, {
  __call = function(_, ...)
    return quat_t():set(...)
  end
})

local mat4 = {
  _type = C.MATH_MAT4,

  unpack = function(m, raw)
    checkmat4(m)
    if raw then
      return
        m.m[0], m.m[1], m.m[2], m.m[3],
        m.m[4], m.m[5], m.m[6], m.m[7],
        m.m[8], m.m[9], m.m[10], m.m[11],
        m.m[12], m.m[13], m.m[14], m.m[15]
    else
      local f = new('float[10]')
      C.mat4_getTransform(m, f + 0, f + 1, f + 2, f + 3, f + 4, f + 5, f + 6, f + 7, f + 8, f + 9)
      return f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[9]
    end
  end,

  set = function(M, ...)
    checkmat4(M)

    if ... == nil then
      return C.mat4_identity(M)
    end

    if select('#', ...) >= 16 then
      M.m[0] , M.m[1] , M.m[2] ,  M.m[3],
      M.m[4] , M.m[5] , M.m[6] ,  M.m[7],
      M.m[8] , M.m[9] , M.m[10], M.m[11],
      M.m[12], M.m[13], M.m[14], M.m[15] = ...
      return M
    end

    if istype(mat4, ...) then
      return C.mat4_set(M, ...)
    end

    local i = 1
    C.mat4_identity(M)

    -- Position
    local x, y, z = ...
    if istype(vec3, x) then
      M.m[12], M.m[13], M.m[14] = x.x, x.y, x.z
      i = i + 1
    else
      assert(type(x) == 'number' and type(y) == 'number' and type(z) == 'number', 'Expected position as 3 numbers or a vec3')
      M.m[12], M.m[13], M.m[14] = x, y, z
      i = i + 3
    end

    -- Scale
    local sx, sy, sz = select(i, ...)
    if istype(vec3, sx) then
      sx, sy, sz = sx.x, sx.y, sx.z
      i = i + 1
    else
      i = i + 3
    end

    -- Rotate
    local angle, ax, ay, az = select(i, ...)
    if istype(quat, angle) then
      C.mat4_rotateQuat(M, angle)
    else
      C.mat4_rotate(M, angle, ax, ay, az)
    end

    return C.mat4_scale(M, sx, sy, sz)
  end,

  identity = function(m)
    checkmat4(m)
    return C.mat4_identity(m)
  end,

  invert = function(m)
    checkmat4(m)
    return C.mat4_invert(m)
  end,

  transpose = function(m)
    checkmat4(m)
    return C.mat4_transpose(m)
  end,

  translate = function(m, x, y, z)
    checkmat4(m)
    if type(x) == 'number' then
      return C.mat4_translate(m, x, y, z)
    else
      checkvec3(x, 1, 'vec3 or number')
      return C.mat4_translate(m, x.x, x.y, x.z)
    end
  end,

  rotate = function(m, angle, ax, ay, az)
    checkmat4(m)
    if type(angle) == 'number' then
      return C.mat4_rotate(m, angle, ax, ay, az)
    else
      checkquat(angle, 1, 'quat or number')
      return C.mat4_rotateQuat(m, angle)
    end
  end,

  scale = function(m, sx, sy, sz)
    checkmat4(m)
    if type(sx) == 'number' then
      return C.mat4_scale(m, sx, sy or sx, sz or sx)
    else
      checkvec3(sx, 1, 'vec3 or number')
      return C.mat4_scale(m, sx.x, sx.y, sx.z)
    end
  end,

  mul = function(m, x, y, z)
    checkmat4(m)
    if istype(mat4, x) then
      return C.mat4_multiply(m, x)
    elseif type(x) == 'number' then
      local f = new('float[3]', x or 0, y or 0, z or 0)
      C.mat4_transform(m, f + 0, f + 1, f + 2)
      return f[0], f[1], f[2]
    else
      checkvec3(x, 2, 'mat4, vec3, or number')
      C.mat4_transform(m, x._p + 0, x._p + 1, x._p + 2)
      return x
    end
  end,

  perspective = function(m, near, far, fov, aspect)
    checkmat4(m)
    return C.mat4_perspective(m, near, far, fov, aspect)
  end,

  orthographic = function(m, left, right, top, bottom, near, far)
    checkmat4(m)
    return C.mat4_orthographic(m, left, right, top, bottom, near, far)
  end,

  __mul = function(m, n)
    checkmat4(m, 1)
    if istype(mat4, n) then
      return C.mat4_multiply(mat4_t():set(m), n)
    else
      checkvec3(n, 2, 'mat4 or vec3')
      local f = new('float[3]', n.x, n.y, n.z)
      C.mat4_transform(m, f + 0, f + 1, f + 2)
      return vec3_t(f[0], f[1], f[2])
    end
  end,

  __tostring = function(m)
    return 'mat4'
  end
}

mat4.__index = mat4
ffi.metatype(mat4_t, mat4)
math.mat4 = setmetatable(mat4, {
  __call = function(_, ...)
    return mat4_t():set(...)
  end
})

function Pool:vec3(...) return C.lovrPoolAllocateVec3(cast('Pool**', self)[0])[0]:set(...) end
function Pool:quat(...) return C.lovrPoolAllocateQuat(cast('Pool**', self)[0])[0]:set(...) end
function Pool:mat4(...) return C.lovrPoolAllocateMat4(cast('Pool**', self)[0])[0]:set(...) end
