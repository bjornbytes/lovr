config = {
  target = nil,
  debug = true,
  strict = true,
  optimize = false,
  sanitize = false,
  luajit = false,
  modules = {
    audio = true,
    data = true,
    event = true,
    filesystem = true,
    graphics = true,
    headset = true,
    math = true,
    physics = true,
    system = true,
    thread = true,
    timer = true
  },
  headsets = {
    simulator = true,
    openvr = false,
    openxr = false,
    oculus = false,
    vrapi = false,
    pico = false,
    webxr = false
  },
  spatializers = {
    simple = true,
    oculus = false,
    phonon = false
  }
}

---> setup

target = config.target or tup.getconfig('TUP_PLATFORM')
target = target == 'macosx' and 'macos' or target
desktop = target == 'win32' or target == 'macos' or target == 'linux'

cc = 'clang'
cxx = 'clang++'

cflags = {
  '-std=c99',
  '-pedantic',
  '-Wall',
  '-Wextra',
  '-Wno-unused-parameter',
  '-fvisibility=hidden',
}

lflags += '-Lbin'

if target == 'win32' then
  cflags += '-D_CRT_SECURE_NO_WARNINGS'
  cflags += '-Wno-language-extension-token'
  extra_outputs += { 'bin/lovr.lib', 'bin/lovr.exp' }
  if config.debug then
    extra_outputs += { 'bin/lovr.pdb', 'bin/lovr.ilk' }
  end
end

if target == 'linux' then
  cflags += '-D_POSIX_C_SOURCE=200809L'
  lflags += '-lm -lpthread -ldl'
  lflags += '-Wl,-rpath,\\$ORIGIN'
end

if target == 'web' then
  cc = 'emcc'
  cxx = 'em++'
  cflags += '-D_POSIX_C_SOURCE=200809L'
  lflags += '-s USE_WEBGL2'
  lflags += '-s FORCE_FILESYSTEM'
  lflags += ([[-s EXPORTED_FUNCTIONS="[
    '_main','_lovrDestroy','_webxr_attach','_webxr_detach','_lovrCanvasCreateFromHandle',
    '_lovrCanvasDestroy','_lovrGraphicsSetBackbuffer','_lovrGraphicsSetViewMatrix',
    '_lovrGraphicsSetProjection'
  ]"]]):gsub('\n', '')
  if config.headsets.webxr then
    lflags += '--js-library src/resources/webxr.js'
  end
  lflags += '--shell-file src/resources/lovr.html'
  extra_outputs += { 'bin/lovr.js', 'bin/lovr.wasm' }
  if config.modules.thread then
    cflags += '-s USE_PTHREADS=1'
    lflags += '-s USE_PTHREADS=1'
    extra_outputs += 'bin/lovr.worker.js'
  end
end

includes = {
  'src',
  'src/modules',
  'src/lib/stdatomic',
}

toggles = {
  debug = '-g',
  strict = '-Werror',
  optimize = '-Os',
  sanitize = '-fsanitize=address,undefined',
}

overrides = {
  glad = '-Wno-pedantic',
  os_android = '-Wno-format-pedantic',
  openvr = '-Wno-unused-variable -Wno-typedef-redefinition -Wno-pedantic',
  vrapi = '-Wno-c11-extensions -Wno-gnu-empty-initializer -Wno-pedantic',
  miniaudio = '-Wno-unused-function',
}

for i, include in ipairs(includes) do
  cflags += '-I' .. include
end

for toggle, flag in pairs(toggles) do
  if config[toggle] then
    cflags += flag
    lflags += flag
  end
end

for file, override in pairs(overrides) do
  _G['cflags_' .. file] = override
end

cflags += desktop and '-DLOVR_GL' or (target == 'android' and '-DLOVR_GLES' or '-DLOVR_WEBGL')

---> deps

if config.luajit then
  cflags += '-Ideps/luajit/src'
else
  cflags += '-Ideps/lua'
  lflags += '-llua'
  lua_cflags += '-fPIC'
  lua_cflags += '-Wno-empty-body'
  lua_lflags += '-lm'
  lua_src = {
    'lapi.c', 'lauxlib.c', 'lbaselib.c', 'lcode.c', 'ldblib.c', 'ldebug.c', 'ldo.c', 'ldump.c',
    'lfunc.c', 'lgc.c', 'linit.c', 'liolib.c', 'llex.c', 'lmathlib.c', 'lmem.c', 'loadlib.c',
    'lobject.c', 'lopcodes.c', 'loslib.c', 'lparser.c', 'lstate.c', 'lstring.c', 'lstrlib.c',
    'ltable.c', 'ltablib.c', 'ltm.c', 'lundump.c', 'lvm.c', 'lzio.c'
  }
  for i = 1, #lua_src do lua_src[i] = 'deps/lua/' .. lua_src[i] end
  tup.foreach_rule(lua_src, '^ CC lua/%b^ $(cc) $(lua_cflags) -c %f -o %o', '.obj/lua/%B.o')
  tup.rule('.obj/lua/*.o', '^ LD %o^ $(cc) $(lua_lflags) -shared -o %o %f', 'bin/liblua.so')
end

if desktop then
  cflags += '-Ideps/glfw/include'
  lflags += '-lglfw'
  glfw_cflags += '-fPIC'
  glfw_cflags += ({ win32 = '-D_GLFW_WIN32', macos = '-D_GLFW_COCOA', linux = '-D_GLFW_X11' })[target]
  glfw_src += { 'init.c', 'context.c', 'input.c', 'monitor.c', 'vulkan.c', 'window.c' }
  glfw_src += ({
    win32 = { 'win32*.c' },
    macos = { 'cocoa*.c', 'posix*.c' },
    linux = { 'x11*.c', 'xkb*.c', 'posix*.c', 'glx*.c', 'egl*.c', 'linux*.c', 'osmesa*.c' }
  })[target]
  for i = 1, #glfw_src do glfw_src[i] = 'deps/glfw/src/' .. glfw_src[i] end
  tup.foreach_rule(glfw_src, '^ CC glfw/%b^ $(cc) $(glfw_cflags) -c %f -o %o', '.obj/glfw/%B.o')
  tup.rule('.obj/glfw/*.o', '^ LD %o^ $(cc) -shared -o %o %f', 'bin/libglfw.so')
end

if config.modules.data then
  cflags_rasterizer += '-Ideps/msdfgen'
  lflags += '-lmsdfgen'
  msdfgen_cflags += '-fPIC'
  msdfgen_cflags += config.optimize and '-O2' or ''
  msdfgen_src += 'deps/msdfgen/core/*.cpp'
  tup.foreach_rule(msdfgen_src, '^ CC msdfgen/%b^ $(cxx) $(msdfgen_cflags) -c %f -o %o', '.obj/msdfgen/%B.o')
  tup.rule('.obj/msdfgen/*.o', '^ LD %o^ $(cxx) -shared -o %o %f', 'bin/libmsdfgen.so')
end

if config.modules.physics then
  cflags += '-Ideps/ode/include'
  lflags += '-lode'

  -- ou
  ode_cflags += '-DMAC_OS_X_VERSION=1030'
  ode_cflags += '-D_OU_NAMESPACE=odeou'
  ode_cflags += '-D_OU_FEATURE_SET=_OU_FEATURE_SET_TLS'
  ode_cflags += '-DdATOMICS_ENABLED=1'
  ode_cflags += '-Ideps/ode/ou/include'
  ode_src += 'deps/ode/ou/src/ou/*.cpp'

  -- ccd
  ode_cflags += '-Ideps/ode/libccd/src'
  ode_cflags += '-Ideps/ode/libccd/src/custom'
  ode_cflags += {
    '-DdLIBCCD_ENABLED',
    '-DdLIBCCD_INTERNAL',
    '-DdLIBCCD_BOX_CYL',
    '-DdLIBCCD_CYL_CYL',
    '-DdLIBCCD_CAP_CYL',
    '-DdLIBCCD_CONVEX_BOX',
    '-DdLIBCCD_CONVEX_CAP',
    '-DdLIBCCD_CONVEX_CYL',
    '-DdLIBCCD_CONVEX_SPHERE',
    '-DdLIBCCD_CONVEX_CONVEX'
  }
  ode_c_src += 'deps/ode/libccd/src/*.c'

  -- OPCODE
  ode_cflags += '-Ideps/ode/OPCODE'
  ode_src += 'deps/ode/OPCODE/*.cpp'
  ode_src += 'deps/ode/OPCODE/Ice/*.cpp'

  -- ode
  ode_cflags += '-fPIC'
  ode_cflags += '-Wno-implicit-int-float-conversion'
  ode_cflags += '-Wno-array-bounds'
  ode_cflags += '-Wno-undefined-var-template'
  ode_cflags += '-Wno-undefined-bool-conversion'
  ode_cflags += '-Wno-unused-value'
  ode_cflags += '-Ideps/ode/include'
  ode_cflags += '-Ideps/ode/ode/src'
  ode_c_src += 'deps/ode/ode/src/*.c'
  ode_src += {
    'deps/ode/ode/src/*.cpp',
    'deps/ode/ode/src/joints/*.cpp'
  }

  for i = #ode_src, 1, -1 do
    if ode_src[i]:match('gimpact') or ode_src[i]:match('dif') then
      table.remove(ode_src, i)
    end
  end

  tup.foreach_rule(ode_c_src, '^ CC ode/%b^ $(cc) $(ode_cflags) -c %f -o %o', '.obj/ode/%B.o')
  tup.foreach_rule(ode_src, '^ CC ode/%b^ $(cxx) $(ode_cflags) -c %f -o %o', '.obj/ode/%B.o')
  tup.rule('.obj/ode/*.o', '^ LD %o^ $(cxx) -shared -o %o %f', 'bin/libode.so')
end

if config.headsets.openvr then
  cflags_openvr += '-Ideps/openvr/headers'
  lflags += '-lopenvr_api'
  openvr_libs = {
    win32 = 'deps/openvr/bin/win64/openvr_api.dll',
    macos = 'deps/openvr/bin/osx32/openvr_api.dll',
    linux = 'deps/openvr/bin/linux64/libopenvr_api.so'
  }
  assert(openvr_libs[target], 'OpenVR is not supported on this system')
  tup.rule(openvr_libs[target], 'cp %f %o', 'bin/%b')
end

if config.headsets.oculus then
  assert(target == 'windows', 'LibOVR is not supported on this platform')
  cflags_oculus += '-Ideps/LibOVR/Include'
  tup.rule('deps/LibOVR/Lib/Windows/x64/Release/VS2017/LibOVR.dll', 'cp %f %o', 'bin/%b')
end

if config.spatializers.oculus then
  cflags_oculus += '-Ideps/AudioSDK/Include'
  ovraudio_libs = {
    win32 = 'deps/AudioSDK/Lib/x64/ovraudio64.dll',
    linux = 'deps/AudioSDK/Lib/Linux64/libovraudio64.so',
    android = 'deps/AudioSDK/Lib/Android/arm64-v8a/libovraudio64.so'
  }
  assert(ovraudio_libs[target], 'Oculus Audio is not supported on this platform')
  tup.rule(ovraudio_libs[target], 'cp %f %o', 'bin/%b')
end

if config.spatializers.phonon then
  cflags_phonon += '-Ideps/phonon/include'
  phonon_libs = {
    win32 = 'deps/phonon/bin/Windows/x64/phonon.dll',
    macos = 'deps/phonon/lib/OSX/libphonon.dylib',
    linux = 'deps/phonon/lib/Linux/x64/libphonon.so',
    android = 'deps/phonon/lib/Android/arm64/libphonon.so'
  }
  assert(phonon_libs[target], 'Phonon is not supported on this system')
  tup.rule(phonon_libs[target], 'cp %f %o', 'bin/%b')
end

---> lovr

src = {
  'src/main.c',
  'src/core/os_' .. target .. '.c',
  'src/core/util.c',
  'src/core/fs.c',
  'src/core/map.c',
  'src/core/zip.c',
  'src/api/api.c',
  'src/api/l_lovr.c'
}

for module, enabled in pairs(config.modules) do
  if enabled then
    src += 'src/modules/' .. module .. '/*.c'
    src += 'src/api/l_' .. module .. '*.c'
  else
    cflags += '-DLOVR_DISABLE_' .. module:upper()
  end
end

for headset, enabled in pairs(config.headsets) do
  if enabled then
    cflags += '-DLOVR_USE_' .. headset:upper()
    src += 'src/modules/headset/xr/' .. headset .. '.c'
  end
end

for spatializer, enabled in pairs(config.spatializers) do
  if enabled then
    cflags += '-DLOVR_ENABLE_' .. spatializer:upper()
    src += 'src/modules/audio/spatializer/' .. spatializer .. '.c'
  end
end

src += 'src/lib/stb/*.c'
src += (config.modules.audio or config.modules.data) and 'src/lib/miniaudio/*.c' or ''
src += config.modules.data and 'src/lib/jsmn/*.c' or ''
src += config.modules.data and 'src/lib/minimp3/*.c' or ''
src += config.modules.graphics and 'src/lib/glad/*.c' or ''
src += config.modules.graphics and 'src/resources/shaders.c' or ''
src += config.modules.math and 'src/lib/noise1234/*.c' or ''
src += config.modules.thread and 'src/lib/tinycthread/*.c' or ''

res += 'src/resources/*.lua'
res += 'src/resources/*.ttf'
res += 'src/resources/*.json'

for i = 1, #res do
  src.extra_inputs += res[i] .. '.h'
end

src.extra_inputs += extra_inputs

obj += '.obj/*.o'
obj.extra_inputs = 'bin/*.so'

---> build

suffix = {
  win32 = '.exe',
  web = '.html'
}

output = { 'bin/lovr' .. (suffix[target] or '') }
output.extra_outputs = extra_outputs

tup.foreach_rule(res, '^ XD %b^ xxd -i %f > %o', '%f.h')
tup.foreach_rule(src, '^ CC %b^ $(cc) $(cflags) $(cflags_%B) -o %o -c %f', '.obj/%B.o')
tup.rule(obj, '^ LD %o^ $(cc) -o %o %f $(lflags)', output)
