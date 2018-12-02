local _Gmath = math
local math, Pool = ...
local ffi = type(jit) == 'table' and jit.status() and require 'ffi'
if not ffi then return end

local C, new, cast, typeof, istype = ffi.C, ffi.new, ffi.cast, ffi.typeof, ffi.istype

ffi.cdef [[
  typedef union { struct { float x; float y; float z; }; float _p[4]; } vec3;
  typedef union { struct { float x; float y; float z; float w; }; float _p[4]; } quat;
  typedef union { struct { float m[16]; }; float _p[16]; } mat4;

  // From math/pool.h
  typedef struct Pool Pool;
  enum { MATH_VEC3, MATH_QUAT, MATH_MAT4 };

  Pool* lovrPoolCreate(size_t count);
  vec3* lovrPoolAllocateVec3(Pool* pool);
  quat* lovrPoolAllocateQuat(Pool* pool);
  mat4* lovrPoolAllocateMat4(Pool* pool);

  Pool* lovrMathGetPool();

  // From lib/math.h
  // Careful, the declarations below are using as the structs above, not the usual C types

  vec3* vec3_normalize(vec3* v);
  float vec3_length(vec3* v);
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

function Pool:vec3(...) return C.lovrPoolAllocateVec3(cast('Pool**', self)[0]):set(...) end
function Pool:quat(...) return C.lovrPoolAllocateQuat(cast('Pool**', self)[0]):set(...) end
function Pool:mat4(...) return C.lovrPoolAllocateMat4(cast('Pool**', self)[0]):set(...) end

local pool = C.lovrMathGetPool()
function math.vec3(...) return C.lovrPoolAllocateVec3(pool):set(...) end
function math.quat(...) return C.lovrPoolAllocateQuat(pool):set(...) end
function math.mat4(...) return C.lovrPoolAllocateMat4(pool):set(...) end

local vec3 = ffi.typeof('vec3')
local quat = ffi.typeof('quat')
local mat4 = ffi.typeof('mat4')

local function checktype(t, name)
  return function(x, i, exp)
    if not istype(t, x) then
      local fn, arg, exp = debug.getinfo(2, 'n').name, i and ('argument #' .. i) or 'self', exp or name
      error(string.format('Bad %s to %s (%s expected, got %s)', arg, fn, exp, type(x)), 3)
    end
  end
end

local checkvec3 = checktype(vec3, 'vec3')
local checkquat = checktype(quat, 'quat')
local checkmat4 = checktype(mat4, 'mat4')

ffi.metatype(vec3, {
  __index = {
    _type = C.MATH_VEC3,

    unpack = function(v)
      checkvec3(v)
      return v.x, v.y, v.z
    end,

    set = function(v, x, y, z)
      checkvec3(v)
      if type(x) == 'number' then
        v.x, v.y, v.z = x, y, z
      else
        checkvec3(x, 1)
        v.x, v.y, v.z = x.x, x.y, x.z
      end
      return v
    end,

    copy = function(v, u)
      checkvec3(v)
      if istype(vec3, u) then
        return u:set(v)
      else
        return math.vec3(v)
      end
    end,

    save = function(v)
      checkvec3(v)
      return vec3(v:unpack())
    end,

    normalize = function(v)
      checkvec3(v)
      C.vec3_normalize(v)
      return v
    end,

    dot = function(v, u)
      checkvec3(v)
      checkvec3(u)
      return v.x * u.x + v.y * u.y + v.z * u.z
    end,

    cross = function(v, u)
      checkvec3(v)
      checkvec3(u)
      C.vec3_cross(v, u)
      return v
    end,

    lerp = function(v, u, t)
      checkvec3(v)
      checkvec3(u)
      C.vec3_lerp(v, u, t)
      return v
    end
  },

  __add = function(v, u) return math.vec3(v.x + u.x, v.y + u.y, v.z + u.z) end,
  __sub = function(v, u) return math.vec3(v.x - u.x, v.y - u.y, v.z - u.z) end,
  __mul = function(v, u) return math.vec3(v.x * u.x, v.y * u.y, v.z * u.z) end,
  __div = function(v, u) return math.vec3(v.x / u.x, v.y / u.y, v.z / u.z) end,
  __unm = function(v) return math.vec3(-v.x, -v.y, -v.z) end,
  __len = function(v) return C.vec3_length(v) end,
  __tostring = function(v) return string.format('(%f, %f, %f)', v.x, v.y, v.z) end
})

ffi.metatype(quat, {
  __index = {
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
          local axis = checkvec3(y)
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
      else
        error('Expected a vec3, quat, mat4, or number')
      end
      return q
    end,

    copy = function(q, r)
      if istype(quat, r) then
        return r:set(q)
      else
        return math.quat(q)
      end
    end,

    save = function(q)
      checkquat(q)
      return quat(quat, q:unpack())
    end,

    normalize = function(q)
      checkquat(q)
      C.quat_normalize(q)
    end,

    slerp = function(q, r, t)
      checkquat(q)
      checkquat(r)
      C.quat_slerp(q, r, t)
    end
  },

  __mul = function(q, r)
    checkquat(q)
    if istype(vec3, r) then
      local v = math.vec3(r)
      C.quat_rotate(q, v)
      return v
    else
      checkquat(r, 'Expected a vec3 or quat')
      local out = math.quat()
      C.quat_mul(C.quat_init(out, q), r)
      return out
    end
  end,

  __len = function(q)
    checkquat(q)
    return C.quat_length(q)
  end,

  __tostring = function(q) return string.format('(%f, %f, %f, %f)', q.x, q.y, q.z, q.w) end
})

ffi.metatype(mat4, {
  __index = {
    _type = C.MATH_MAT4,

    unpack = function(m)
      checkmat4(m)
      return -- yum
        m.m[0], m.m[1], m.m[2], m.m[3],
        m.m[4], m.m[5], m.m[6], m.m[7],
        m.m[8], m.m[9], m.m[10], m.m[11],
        m.m[12], m.m[13], m.m[14], m.m[15]
    end,

    set = function(m, x, ...)
      checkmat4(m)
      if not x then
        m:identity()
      elseif not ... and type(x) == 'number' then
        m.m[0], m.m[5], m.m[10], m.m[15] = x, x, x, x
      elseif ... then
        m.m[0], m.m[1], m.m[2], m.m[3],
        m.m[4], m.m[5], m.m[6], m.m[7],
        m.m[8], m.m[9], m.m[10], m.m[11],
        m.m[12], m.m[13], m.m[14], m.m[15] = ...
      end
      return m
    end,

    copy = function(m)
      checkmat4(m)
      return math.mat4(m:unpack())
    end,

    save = function(m)
      checkmat4(m)
      return mat4(m:unpack())
    end,

    identity = function(m)
      checkmat4(m)
      C.mat4_identity(m)
    end,

    inverse = function(m)
      checkmat4(m)
      local out = math.mat4()
      C.mat4_invert(C.mat4_set(out, m))
      return out
    end,

    transpose = function(m)
      checkmat4(m)
      local out = math.mat4()
      C.mat4_transpose(C.mat4_set(out, m))
      return out
    end,

    translate = function(m, x, y, z)
      checkmat4(m)
      if type(x) == 'number' then
        C.mat4_translate(m, x, y, z)
      else
        checkvec3(x, 1, 'vec3 or number')
        C.mat4_translate(m, x.x, x.y, x.z)
      end
    end,

    rotate = function(m, angle, ax, ay, az)
      checkmat4(m)
      if type(angle) == 'number' then
        C.mat4_rotate(m, angle, ax, ay, az)
      else
        checkquat(angle, 1, 'quat or number')
        C.mat4_rotateQuat(m, angle)
      end
    end,

    scale = function(m, sx, sy, sz)
      checkmat4(m)
      if type(sx) == 'number' then
        C.mat4_scale(m, sx, sy, sz)
      else
        checkvec3(sx, 1, 'vec3 or number')
        C.mat4_scale(m, sx.x, sx.y, sx.z)
      end
    end,

    getTransform = function(m)
      checkmat4(m)
      local f = new('float[10]')
      C.mat4_getTransform(m, f + 0, f + 1, f + 2, f + 3, f + 4, f + 5, f + 6, f + 7, f + 8, f + 9)
      return f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[9]
    end,

    setTransform = function(m, x, y, z, sx, sy, sz, angle, ax, ay, az)
      checkmat4(m)
      C.mat4_setTransform(m, x or 0, y or 0, z or 0, sx or 1, sy or sx or 1, sz or sx or 1, angle or 0, ax or 0, ay or 1, az or 0)
      return m
    end,

    perspective = function(m, near, far, fov, aspect)
      checkmat4(m)
      C.mat4_perspective(m, near, far, fov, aspect)
      return m
    end,

    orthographic = function(m, left, right, top, bottom, near, far)
      checkmat4(m)
      C.mat4_orthographic(m, left, right, top, bottom, near, far)
      return m
    end
  },

  __mul = function(m, n)
    checkmat4(m)
    if istype(mat4, n) then
      local out = math.mat4(m)
      C.mat4_multiply(out, n)
      return out
    else
      checkvec3(n, 1, 'mat4 or vec3')
      local f = new('float[3]', n.x, n.y, n.z)
      C.mat4_transform(m, f + 0, f + 1, f + 2)
      return math.vec3(f[0], f[1], f[2])
    end
  end
})
