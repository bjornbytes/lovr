config = {
  target = 'native',
  debug = true,
  optimize = false,
  supercharge = false,
  sanitize = false,
  strict = true,
  glfw = true,
  luajit = false,
  glslang = true,
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
    desktop = true,
    openxr = false,
    webxr = false
  },
  renderers = {
    vulkan = true,
    webgpu = false
  },
  spatializers = {
    simple = true,
    oculus = false,
    phonon = false
  },
  android = {
    sdk = '/path/to/sdk',
    ndk = '/path/to/ndk',
    version = 29,
    buildtools = '30.0.3',
    keystore = '/path/to/keystore',
    keystorepass = 'pass:password',
    manifest = nil,
    package = nil,
    project = nil
  }
}

-- config notes:
-- target can be native or win32/macos/linux/android/wasm
-- supercharge adds dangerous/aggressive optimizations that may reduce stability
-- sanitize adds checks for memory leaks and undefined behavior (reduces performance)
-- strict will make warnings fail the build
-- luajit and headsets.openxr should be a path to a folder with the lib (tup can't build them yet)
-- android.package should be something like 'org.lovr.app'
-- android.project is a path to a lovr project folder that will be included in the apk
-- tup.config can also be used to override properties without modifying this file:
-- create a tup.config file and add lines like CONFIG_<KEY>=<val> to override properties
-- boolean values use y and n, numbers and unquoted strings are also supported
-- subtables should be formatted as CONFIG_MODULE_<m>=y, CONFIG_ANDROID_VERSION=29, etc.

function merge(t, prefix)
  for k, v in pairs(t) do
    if type(v) == 'table' then
      merge(v, k:gsub('s$', ''):upper() .. '_')
    else
      local str = tup.getconfig((prefix or '') .. k:upper())
      if str == 'y' or str == 'n' then
        t[k] = str == 'y'
      elseif #str > 0 then
        t[k] = str
      end
    end
  end
end

merge(config)

---> setup

cc = 'clang'
cxx = 'clang++'
host = tup.getconfig('TUP_PLATFORM'):gsub('macosx', 'macos')
target = config.target == 'native' and host or config.target

flags = {
  '-fPIE',
  config.debug and '-g' or '',
  config.optimize and '-Os' or '',
  config.supercharge and '-flto -march=native -DLOVR_UNCHECKED' or '',
  config.sanitize and '-fsanitize=address' or '',
}

cflags = {
  '-std=c11 -pedantic',
  '-Wall -Wextra -Wno-unused-parameter',
  config.strict and '-Werror' or '',
  config.optimize and '-fdata-sections -ffunction-sections' or '',
  '-fdiagnostics-color=always',
  '-fvisibility=hidden',
  '-Ietc',
  '-Isrc',
  '-Isrc/modules',
  '-Isrc/lib/stdatomic'
}

bin = target == 'android' and 'bin/apk/lib/arm64-v8a' or 'bin'

lflags = '-L' .. bin
lflags += not config.debug and '-Wl,-s' or ''
lflags += config.optimize and (target == 'macos' and '-Wl,-dead_strip' or '-Wl,--gc-sections') or ''
lflags += '-rdynamic'

if target == 'win32' then
  cflags += '-D_CRT_SECURE_NO_WARNINGS'
  cflags += '-DWINVER=0x0600' -- Vista
  cflags += '-D_WIN32_WINNT=0x0600'
  lflags += '-lshell32 -lole32 -luuid'
  lflags += config.debug and '-Wl,--subsystem,console' or '-Wl,--subsystem,windows'
  if not cc:match('mingw') then
    extras += { 'bin/lovr.lib', 'bin/lovr.exp' }
    if config.debug then
      extras += { 'bin/lovr.pdb', 'bin/lovr.ilk' }
    end
  end
end

if target == 'macos' then
  cflags_os_macos += '-xobjective-c'
  lflags += '-Wl,-rpath,@executable_path'
  lflags += '-lobjc'
  lflags += '-framework AVFoundation'
end

if target == 'linux' then
  cflags += '-D_POSIX_C_SOURCE=200809L'
  cflags += '-D_DEFAULT_SOURCE'
  lflags += '-lm -lpthread -ldl'
  lflags += '-Wl,-rpath,\\$ORIGIN'
end

if target == 'wasm' then
  cflags += '-std=gnu11'
  cflags += '-D_POSIX_C_SOURCE=200809L'
  lflags += '-s FORCE_FILESYSTEM'
  exported_functions = { '_main', '_lovrDestroy' }
  if config.headsets.webxr then
    exported_functions += { '_webxr_attach', '_webxr_detach' }
    lflags += '--js-library etc/webxr.js'
  end
  lflags += '--shell-file etc/lovr.html'
  lflags += '-s EXPORTED_FUNCTIONS=' .. table.concat(exported_functions, ',')
  extras += { 'bin/lovr.js', 'bin/lovr.wasm' }
  if config.renderers.webgpu then
    lflags += '-s USE_WEBGPU=1'
  end
  if config.modules.thread then
    cflags += '-s USE_PTHREADS=1'
    lflags += '-s USE_PTHREADS=1'
    extras += 'bin/lovr.worker.js'
  end
end

if target == 'android' then
  assert(config.headsets.openxr, 'You probably want to enable OpenXR')
  hosts = { win32 = 'windows-x86_64', macos = 'darwin-x86_64', linux = 'linux-x86_64' }
  cc = ('%s/toolchains/llvm/prebuilt/%s/bin/clang'):format(config.android.ndk, hosts[host])
  cxx = cc .. '++'
  flags += '--target=aarch64-linux-android' .. config.android.version
  flags += config.debug and '-funwind-tables' or ''
  cflags += '-D_POSIX_C_SOURCE=200809L'
  cflags += ('-I%s/sources/android/native_app_glue'):format(config.android.ndk)
  lflags += '-shared -landroid'
end

troublemakers = {
  os_android = '-Wno-format-pedantic',
  miniaudio = '-Wno-unused-function -Wno-pedantic',
}

for file, flags in pairs(troublemakers) do
  _G['cflags_' .. file] = flags
end

---> deps

local function lib(name)
  return (target == 'win32' and '$(bin)/%s.dll' or '$(bin)/lib%s.so'):format(name)
end

local function copy(from, to)
  tup.rule(from, '^ CP %b^ cp %f %o', to)
end

if config.luajit then
  if type(config.luajit) ~= 'string' then
    error('Sorry, building LuaJIT is not supported yet.  However, you can set config.luajit to a path to a folder containing the LuaJIT library.')
  end
  cflags += '-Ideps/luajit/src'
  lflags += '-L' .. config.luajit
  lflags += '-lluajit'
else
  cflags += '-Ideps/lua'
  lflags += '-llua'
  lua_cflags += '-fPIC'
  lua_cflags += '-Wno-empty-body'
  lua_lflags += '-shared'
  lua_lflags += '-lm'
  lua_src = {
    'lapi.c', 'lauxlib.c', 'lbaselib.c', 'lcode.c', 'ldblib.c', 'ldebug.c', 'ldo.c', 'ldump.c',
    'lfunc.c', 'lgc.c', 'linit.c', 'liolib.c', 'llex.c', 'lmathlib.c', 'lmem.c', 'loadlib.c',
    'lobject.c', 'lopcodes.c', 'loslib.c', 'lparser.c', 'lstate.c', 'lstring.c', 'lstrlib.c',
    'ltable.c', 'ltablib.c', 'ltm.c', 'lundump.c', 'lvm.c', 'lzio.c'
  }
  lua_cflags += ({ linux = '-DLUA_USE_LINUX', android = '-DLUA_USE_LINUX', macos = '-DLUA_USE_MACOSX' })[target] or ''
  for i = 1, #lua_src do lua_src[i] = 'deps/lua/' .. lua_src[i] end
  tup.foreach_rule(lua_src, '^ CC lua/%b^ $(cc) $(flags) $(lua_cflags) -c %f -o %o', '.obj/lua/%B.o')
  tup.rule('.obj/lua/*.o', '^ LD %o^ $(cc) $(flags) -o %o %f $(lua_lflags)', lib('lua'))
end

if config.glfw and (target == 'win32' or target == 'macos' or target == 'linux') then
  cflags += '-Ideps/glfw/include'
  cflags += '-DLOVR_USE_GLFW'
  lflags += '-lglfw'

  glfw_cflags += '-fPIC'
  glfw_cflags += ({ win32 = '-D_GLFW_WIN32', macos = '-D_GLFW_COCOA', linux = '-D_GLFW_X11' })[target]
  glfw_src += { 'init.c', 'context.c', 'input.c', 'monitor.c', 'vulkan.c', 'window.c' }
  glfw_src += ({
    win32 = { 'win32*.c', 'wgl*.c', 'egl*.c', 'osmesa*.c' },
    macos = { 'cocoa*.c', 'cocoa*.m', 'posix_thread.c', 'egl*.c', 'nsgl*.m', 'osmesa*.c' },
    linux = { 'x11*.c', 'xkb*.c', 'posix*.c', 'glx*.c', 'egl*.c', 'linux*.c', 'osmesa*.c' }
  })[target]
  for i = 1, #glfw_src do glfw_src[i] = 'deps/glfw/src/' .. glfw_src[i] end
  glfw_lflags += '-shared'
  glfw_lflags += target == 'win32' and '-lgdi32' or ''
  glfw_lflags += target == 'macos' and '-lobjc -framework Cocoa -framework IOKit -framework CoreFoundation' or ''
  tup.foreach_rule(glfw_src, '^ CC glfw/%b^ $(cc) $(flags) $(glfw_cflags) -c %f -o %o', '.obj/glfw/%B.o')
  tup.rule('.obj/glfw/*.o', '^ LD %o^ $(cc) $(flags) -o %o %f $(glfw_lflags)', lib('glfw'))
end

if config.modules.graphics and config.glslang then
  cflags_graphics += '-Ideps/glslang/glslang/Include'
  cflags_graphics += '-Ideps/glslang/StandAlone'
  cflags += '-DLOVR_USE_GLSLANG'
  lflags += '-lglslang'

  glslang_cflags += '-fPIC'
  glslang_cflags += '-fno-exceptions'
  glslang_cflags += '-fno-rtti'
  glslang_cflags += '-Ideps/glslang'
  glslang_lflags += '-shared'
  glslang_lflags += '-static-libstdc++'
  glslang_src += 'deps/glslang/OGLCompilersDLL/*.cpp'
  glslang_src += 'deps/glslang/glslang/CInterface/*.cpp'
  glslang_src += 'deps/glslang/glslang/MachineIndependent/*.cpp'
  glslang_src += 'deps/glslang/glslang/MachineIndependent/preprocessor/*.cpp'
  glslang_src += ('deps/glslang/glslang/OSDependent/%s/ossource.cpp'):format(target == 'win32' and 'Windows' or 'Unix')
  glslang_src += 'deps/glslang/glslang/GenericCodeGen/*.cpp'
  glslang_src += 'deps/glslang/SPIRV/GlslangToSpv.cpp'
  glslang_src += 'deps/glslang/SPIRV/Logger.cpp'
  glslang_src += 'deps/glslang/SPIRV/SpvTools.cpp'
  glslang_src += 'deps/glslang/SPIRV/SpvBuilder.cpp'
  glslang_src += 'deps/glslang/SPIRV/SpvPostProcess.cpp'
  glslang_src += 'deps/glslang/SPIRV/InReadableOrder.cpp'
  glslang_src += 'deps/glslang/SPIRV/CInterface/spirv_c_interface.cpp'
  glslang_src += 'deps/glslang/StandAlone/resource_limits_c.cpp'
  glslang_src += 'deps/glslang/StandAlone/ResourceLimits.cpp'

  tup.foreach_rule(glslang_src, '^ CC glslang/%b^ $(cc) $(flags) $(glslang_cflags) -c %f -o %o', '.obj/glslang/%B.o')
  tup.rule('.obj/glslang/*.o', '^ LD %o^ $(cxx) $(flags) -o %o %f $(glslang_lflags)', lib('glslang'))
end

if config.modules.data then
  cflags_rasterizer += '-Ideps/msdfgen'
  lflags += '-lmsdfgen'

  msdfgen_cflags += '-fPIC'
  msdfgen_src += 'deps/msdfgen/core/*.cpp'
  tup.foreach_rule(msdfgen_src, '^ CC msdfgen/%b^ $(cxx) $(flags) $(msdfgen_cflags) -c %f -o %o', '.obj/msdfgen/%B.o')
  tup.rule('.obj/msdfgen/*.o', '^ LD %o^ $(cxx) $(flags) -shared -static-libstdc++ -o %o %f', lib('msdfgen'))
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
  ode_cflags += '-Wno-implicit-float-conversion'
  ode_cflags += '-Wno-array-bounds'
  ode_cflags += '-Wno-undefined-var-template'
  ode_cflags += '-Wno-undefined-bool-conversion'
  ode_cflags += '-Wno-unused-value'
  ode_cflags += '-Wno-null-dereference'
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

  tup.foreach_rule(ode_c_src, '^ CC ode/%b^ $(cc) $(flags) $(ode_cflags) -c %f -o %o', '.obj/ode/%B.o')
  tup.foreach_rule(ode_src, '^ CC ode/%b^ $(cxx) $(flags) $(ode_cflags) -c %f -o %o', '.obj/ode/%B.o')
  tup.rule('.obj/ode/*.o', '^ LD %o^ $(cxx) $(flags) -shared -static-libstdc++ -o %o %f', lib('ode'))
end

if config.headsets.openxr then
  if target == 'android' then
    cflags_headset_openxr += '-Ideps/openxr/include'
    lflags += '-lopenxr_loader'
    copy('deps/oculus-openxr/Libs/Android/arm64-v8a/Release/libopenxr_loader.so', '$(bin)/%b')
  else
    if type(config.headsets.openxr) ~= 'string' then
      error('Sorry, building OpenXR is not supported yet.  However, you can set config.headsets.openxr to a path to a folder containing the OpenXR library.')
    end
    cflags_headset_openxr += '-Ideps/openxr/include'
    lflags += '-L' .. config.headsets.openxr
    lflags += '-lopenxr_loader'
  end
end

if config.spatializers.oculus then
  cflags_headset_oculus += '-Ideps/AudioSDK/Include'
  ovraudio_libs = {
    win32 = 'deps/AudioSDK/Lib/x64/ovraudio64.dll',
    linux = 'deps/AudioSDK/Lib/Linux64/libovraudio64.so',
    android = 'deps/AudioSDK/Lib/Android/arm64-v8a/libovraudio64.so'
  }
  assert(ovraudio_libs[target], 'Oculus Audio is not supported on this target')
  copy(ovraudio_libs[target], '$(bin)/%b')
end

if config.spatializers.phonon then
  cflags_spatializer_phonon += '-Ideps/phonon/include'
  phonon_libs = {
    win32 = 'deps/phonon/bin/Windows/x64/phonon.dll',
    macos = 'deps/phonon/lib/OSX/libphonon.dylib',
    linux = 'deps/phonon/lib/Linux/x64/libphonon.so',
    android = 'deps/phonon/lib/Android/arm64/libphonon.so'
  }
  assert(phonon_libs[target], 'Phonon is not supported on this target')
  copy(phonon_libs[target], '$(bin)/%b')
end

---> lovr

src = {
  'src/main.c',
  'src/util.c',
  'src/core/fs.c',
  ('src/core/os_%s.c'):format(target),
  'src/core/spv.c',
  'src/core/zip.c',
  'src/api/api.c',
  'src/api/l_lovr.c'
}

for module, enabled in pairs(config.modules) do
  if enabled then
    override = { audio = 'src/modules/audio/audio.c', headset = 'src/modules/headset/headset.c' } -- TODO
    src += override[module] or ('src/modules/%s/*.c'):format(module)
    src += ('src/api/l_%s*.c'):format(module)
  else
    cflags += '-DLOVR_DISABLE_' .. module:upper()
  end
end

for headset, enabled in pairs(config.headsets) do
  if enabled then
    cflags += '-DLOVR_USE_' .. headset:upper()
    src += ('src/modules/headset/headset_%s.c'):format(headset)
  end
end

for renderer, enabled in pairs(config.renderers) do
  if enabled then
    local code = renderer:gsub('vulkan', 'vk')
    cflags += '-DLOVR_' .. code:upper()
    src += 'src/core/gpu_' .. code .. '.c'
  end
end

for spatializer, enabled in pairs(config.spatializers) do
  if enabled then
    cflags += ('-DLOVR_ENABLE_%s_SPATIALIZER'):format(spatializer:upper())
    src += ('src/modules/audio/spatializer_%s.c'):format(spatializer)
  end
end

src += 'src/lib/stb/*.c'
src += (config.modules.audio or config.modules.data) and 'src/lib/miniaudio/*.c' or nil
src += config.modules.data and 'src/lib/jsmn/*.c' or nil
src += config.modules.data and 'src/lib/minimp3/*.c' or nil
src += config.modules.math and 'src/lib/noise/*.c' or nil
src += config.modules.thread and 'src/lib/tinycthread/*.c' or nil

-- embed resource files with xxd

res = { 'etc/boot.lua', 'etc/nogame.lua', 'etc/*.ttf', 'etc/shaders/*.glsl' }
tup.foreach_rule(res, '^ XD %b^ xxd -i %f > %o', '%f.h')

for i, pattern in ipairs(res) do
  src.extra_inputs += pattern .. '.h'
end

-- shaders

vert = 'etc/shaders/*.vert'
frag = 'etc/shaders/*.frag'
comp = 'etc/shaders/*.comp'

function compileShaders(stage)
  pattern = 'etc/shaders/*.' .. stage
  tup.foreach_rule(pattern, 'glslangValidator --quiet -Os --target-env vulkan1.1 --vn lovr_shader_%B_' .. stage .. ' -o %o %f', '%f.h')
end

compileShaders('vert')
compileShaders('frag')
compileShaders('comp')
src.extra_inputs += 'etc/shaders/*.h'

-- compile

tup.foreach_rule(src, '^ CC %b^ $(cc) $(flags) $(cflags) $(cflags_%B) -o %o -c %f', '.obj/%B.o')

-- link final output

outputs = {
  win32 = '$(bin)/lovr.exe',
  macos = '$(bin)/lovr',
  linux = '$(bin)/lovr',
  android = '$(bin)/liblovr.so',
  wasm = '$(bin)/lovr.html'
}

obj = { '.obj/*.o', extra_inputs = lib('*') }
output = { outputs[target], extra_outputs = extras }
tup.rule(obj, '^ LD %o^ $(cc) $(flags) -o %o %f $(lflags)', output)

---> apk

if target == 'android' then
  for _, key in pairs({ 'manifest', 'package', 'project' }) do
    local value = tup.getconfig('ANDROID_' .. key:upper())
    config.android[key] = #value > 0 and value or config.android[key]
  end

  java = 'bin/Activity.java'
  class = 'org/lovr/app/Activity.class'
  binclass = 'bin/' .. class
  jar = 'bin/lovr.jar'
  dex = 'bin/apk/classes.dex'
  unaligned = 'bin/.lovr.apk.unaligned'
  unsigned = 'bin/.lovr.apk.unsigned'
  apk = 'bin/lovr.apk'

  manifest = config.android.manifest or 'etc/AndroidManifest.xml'
  package = config.android.package and #config.android.package > 0 and ('--rename-manifest-package ' .. config.android.package) or ''
  project = config.android.project and #config.android.project > 0 and ('-A ' .. config.android.project) or ''

  version = config.android.version
  ks = config.android.keystore
  kspass = config.android.keystorepass
  androidjar = ('%s/platforms/android-%d/android.jar'):format(config.android.sdk, version)
  tools = config.android.sdk .. '/build-tools/' .. config.android.buildtools

  copy(manifest, 'bin/AndroidManifest.xml')
  copy('etc/Activity.java', java)
  tup.rule(java, '^ JAVAC %b^ javac -classpath $(androidjar) -d bin %f', binclass)
  tup.rule(binclass, '^ JAR %b^ jar -cf %o -C bin $(class)', jar)
  tup.rule(jar, '^ D8 %b^ $(tools)/d8 --min-api $(version) --output bin/apk %f', dex)
  tup.rule(
    { 'bin/AndroidManifest.xml', extra_inputs = { lib('*'), dex } },
    '^ AAPT %b^ $(tools)/aapt package $(package) -F %o -M %f -0 so -I $(androidjar) $(project) bin/apk',
    unaligned
  )
  tup.rule(unaligned, '^ ZIPALIGN %f^ $(tools)/zipalign -f -p 4 %f %o', unsigned)
  tup.rule(unsigned, '^ APKSIGNER %o^ $(tools)/apksigner sign --ks $(ks) --ks-pass $(kspass) --out %o %f', { apk, extra_outputs = 'bin/lovr.apk.idsig' })
end
