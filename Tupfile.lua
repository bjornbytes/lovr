-- All the values here can be overridden in tup.config, or by using tup variants
-- To override subtables in tup.config, use singular subkeys (e.g. CONFIG_MODULE_AUDIO=n)
config = {
  target = 'native', -- can be win32, macos, linux, android, wasm, or native
  debug = true,
  optimize = false,
  supercharge = false, -- enables dangerous optimizations, plus link time optimization
  sanitize = false, -- adds runtime checks to detect memory leaks and undefined behavior (slower)
  strict = false, -- makes warnings fail the build
  luajit = false, -- path to folder with LuaJIT library, or false to use PUC Lua
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
    openvr = false,
    openxr = false, -- if provided, should be path to folder containing OpenXR loader library
    oculus = false,
    vrapi = false,
    pico = false,
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
    manifest = nil, -- path to custom AndroidManifest.xml
    package = nil, -- package id, like org.lovr.app
    project = nil -- path to LÖVR project to include in apk
  }
}

local function getConfig(key, default)
  local value = tup.getconfig(key)
  if value == 'y' then return true
  elseif value == 'n' then return false
  elseif #value > 0 then return value
  else return default end
end

-- merge tup.config into config table
for k, v in pairs(config) do
  if type(v) == 'table' then
    for kk, vv in pairs(v) do
      local key = k:match('(.-)s?$'):upper() .. '_' .. kk:upper()
      v[kk] = getConfig(key, v[kk])
    end
  else
    config[k] = getConfig(k:upper(), config[k])
  end
end

---> setup

target = config.target == 'native' and tup.getconfig('TUP_PLATFORM') or config.target
if target == 'macosx' then target = 'macos' end

cc = 'clang'
cxx = 'clang++'

cflags = {
  '-std=c99',
  '-pedantic',
  '-Wall',
  '-Wextra',
  config.strict and '-Werror' or '',
  '-Wno-unused-parameter',
  '-fdiagnostics-color=always',
  '-fvisibility=hidden',
  config.optimize and '-fdata-sections -ffunction-sections' or '',
  '-Isrc',
  '-Isrc/modules',
  '-Isrc/lib/stdatomic',
}

bin = target == 'android' and 'bin/apk/lib/arm64-v8a' or 'bin'
lflags = '-L' .. bin
lflags += not config.debug and '-Wl,-s' or ''
lflags += config.optimize and (target == 'macos' and '-Wl,-dead_strip' or '-Wl,--gc-sections') or ''

base_flags = {
  config.debug and '-g' or '',
  config.optimize and '-Os' or '',
  config.supercharge and '-flto -march=native -DLOVR_UNCHECKED' or '',
  config.sanitize and '-fsanitize=address,undefined' or ''
}

if target == 'win32' then
  cflags += '-D_CRT_SECURE_NO_WARNINGS'
  cflags += '-DWINVER=0x0600' -- Vista
  cflags += '-D_WIN32_WINNT=0x0600'
  lflags += '-lshell32 -lole32 -luuid'
  lflags += config.debug and '-Wl,--subsystem,console' or '-Wl,--subsystem,windows'
  if not cc:match('mingw') then
    extra_outputs += { 'bin/lovr.lib', 'bin/lovr.exp' }
    if config.debug then
      extra_outputs += { 'bin/lovr.pdb', 'bin/lovr.ilk' }
    end
  end
end

if target == 'macos' then
  lflags += '-Wl,-rpath,@executable_path'
  lflags += '-lobjc'
end

if target == 'linux' then
  cflags += '-DLOVR_LINUX_X11'
  cflags += '-D_POSIX_C_SOURCE=200809L'
  cflags += '-D_DEFAULT_SOURCE'
  lflags += '-lm -lpthread -ldl'
  lflags += '-Wl,-rpath,\\$ORIGIN'
end

if target == 'wasm' then
  cc = 'emcc'
  cxx = 'em++'
  cflags += '-D_POSIX_C_SOURCE=200809L'
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

if target == 'android' then
  assert(config.headsets.vrapi or config.headsets.pico or config.headsets.openxr, 'Please enable vrapi, pico, or openxr')
  hosts = { win32 = 'windows-x86_64', macos = 'darwin-x86_64', linux = 'linux-x86_64' }
  host = hosts[tup.getconfig('TUP_PLATFORM')]
  cc = ('%s/toolchains/llvm/prebuilt/%s/bin/clang'):format(config.android.ndk, host)
  cxx = cc .. '++'
  base_flags += '--target=aarch64-linux-android' .. config.android.version
  base_flags += config.debug and '-funwind-tables' or ''
  cflags += '-D_POSIX_C_SOURCE=200809L'
  cflags += '-I' .. config.android.ndk .. '/sources/android/native_app_glue'
  lflags += '-shared -landroid'
end

overrides = {
  os_android = '-Wno-format-pedantic',
  openvr = '-Wno-unused-variable -Wno-typedef-redefinition -Wno-pedantic',
  vrapi = '-Wno-c11-extensions -Wno-gnu-empty-initializer -Wno-pedantic',
  miniaudio = '-Wno-unused-function',
}

for file, override in pairs(overrides) do
  _G['cflags_' .. file] = override
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
  for i = 1, #lua_src do lua_src[i] = 'deps/lua/' .. lua_src[i] end
  tup.foreach_rule(lua_src, '^ CC lua/%b^ $(cc) $(base_flags) $(lua_cflags) -c %f -o %o', '.obj/lua/%B.o')
  tup.rule('.obj/lua/*.o', '^ LD %o^ $(cc) $(base_flags) -o %o %f $(lua_lflags)', lib('lua'))
end

if target == 'win32' or target == 'macos' or target == 'linux' then
  cflags += '-Ideps/glfw/include'
  lflags += '-lglfw'

  glfw_cflags += '-fPIC'
  glfw_cflags += ({ win32 = '-D_GLFW_WIN32', macos = '-D_GLFW_COCOA', linux = '-D_GLFW_X11' })[target]
  glfw_src += { 'init.c', 'context.c', 'input.c', 'monitor.c', 'vulkan.c', 'window.c' }
  glfw_src += ({
    win32 = { 'win32*.c', 'wgl*.c', 'egl*.c', 'osmesa*.c' },
    macos = { 'cocoa*.c', 'posix*.c' },
    linux = { 'x11*.c', 'xkb*.c', 'posix*.c', 'glx*.c', 'egl*.c', 'linux*.c', 'osmesa*.c' }
  })[target]
  for i = 1, #glfw_src do glfw_src[i] = 'deps/glfw/src/' .. glfw_src[i] end
  glfw_lflags += '-shared'
  glfw_lflags += target == 'win32' and '-lgdi32' or ''
  tup.foreach_rule(glfw_src, '^ CC glfw/%b^ $(cc) $(base_flags) $(glfw_cflags) -c %f -o %o', '.obj/glfw/%B.o')
  tup.rule('.obj/glfw/*.o', '^ LD %o^ $(cc) $(base_flags) -o %o %f $(glfw_lflags)', lib('glfw'))
end

if config.modules.data then
  cflags_rasterizer += '-Ideps/msdfgen'
  lflags += '-lmsdfgen'

  msdfgen_cflags += '-fPIC'
  msdfgen_src += 'deps/msdfgen/core/*.cpp'
  tup.foreach_rule(msdfgen_src, '^ CC msdfgen/%b^ $(cxx) $(base_flags) $(msdfgen_cflags) -c %f -o %o', '.obj/msdfgen/%B.o')
  tup.rule('.obj/msdfgen/*.o', '^ LD %o^ $(cxx) $(base_flags) -shared -static-libstdc++ -o %o %f', lib('msdfgen'))
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

  tup.foreach_rule(ode_c_src, '^ CC ode/%b^ $(cc) $(base_flags) $(ode_cflags) -c %f -o %o', '.obj/ode/%B.o')
  tup.foreach_rule(ode_src, '^ CC ode/%b^ $(cxx) $(base_flags) $(ode_cflags) -c %f -o %o', '.obj/ode/%B.o')
  tup.rule('.obj/ode/*.o', '^ LD %o^ $(cxx) $(base_flags) -shared -static-libstdc++ -o %o %f', lib('ode'))
end

if config.headsets.openvr then
  cflags_openvr += '-Ideps/openvr/headers'
  lflags += '-lopenvr_api'
  openvr_libs = {
    win32 = 'deps/openvr/bin/win64/openvr_api.dll',
    macos = 'deps/openvr/bin/osx32/openvr_api.dll',
    linux = 'deps/openvr/bin/linux64/libopenvr_api.so'
  }
  assert(openvr_libs[target], 'OpenVR is not supported on this target')
  copy(openvr_libs[target], '$(bin)/%b')
end

if config.headsets.openxr then
  if target == 'android' then
    cflags_headset_openxr += '-Ideps/OpenXR-Oculus/Include'
    lflags += 'lopenxr_loader'
    copy('deps/OpenXR-Oculus/Libs/Android/arm64-v8a/Release/libopenxr_loader.so', '$(bin)/%b')
  else
    if type(config.headsets.openxr) ~= 'string' then
      error('Sorry, building OpenXR is not supported yet.  However, you can set config.headsets.openxr to a path to a folder containing the OpenXR library.')
    end
    cflags_headset_openxr += '-Ideps/openxr/include'
    lflags += '-L' .. config.headsets.openxr
    lflags += '-lopenxr_loader'
  end
end

if config.headsets.oculus then
  assert(target == 'windows', 'LibOVR is not supported on this target')
  cflags_headset_oculus += '-Ideps/LibOVR/Include'
  copy('deps/LibOVR/LibWindows/x64/Release/VS2017/LibOVR.dll', lib('LibOVR'))
end

if config.headsets.vrapi then
  assert(target == 'android', 'VrApi is not supported on this target')
  cflags_headset_vrapi += '-Ideps/oculus-mobile/VrApi/Include'
  cflags_headset_vrapi += '-Wno-gnu-empty-initializer'
  cflags_headset_vrapi += '-Wno-c11-extensions'
  cflags_headset_vrapi += '-Wno-pedantic'
  lflags += '-lvrapi'
  copy('deps/oculus-mobile/VrApi/Libs/Android/arm64-v8a/Release/libvrapi.so', '$(bin)/%b')
end

if config.headsets.pico then
  assert(target == 'android', 'Pico is not supported on this target')
  lflags += '-lPvr_NativeSDK'
  copy('deps/pico/jni/arm64-v8a/libPvr_NativeSDK.so', '$(bin)/%b')
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
  'src/core/os_' .. target .. '.c',
  'src/core/util.c',
  'src/core/fs.c',
  'src/core/map.c',
  'src/core/zip.c',
  'src/api/api.c',
  'src/api/l_lovr.c'
}

-- TODO rearrange source so that things can be globbed/matched consistently
module_src = {
  audio = 'src/modules/audio/audio.c',
  data = 'src/modules/data/*.c',
  event = 'src/modules/event/*.c',
  filesystem = 'src/modules/filesystem/*.c',
  graphics = 'src/modules/graphics/*.c',
  headset = 'src/modules/headset/headset.c',
  math = 'src/modules/math/*.c',
  physics = 'src/modules/physics/*.c',
  system = 'src/modules/system/*.c',
  thread = 'src/modules/thread/*.c',
  timer = 'src/modules/timer/*.c'
}

for module, enabled in pairs(config.modules) do
  if enabled then
    src += module_src[module]
    src += 'src/api/l_' .. module .. '*.c'
  else
    cflags += '-DLOVR_DISABLE_' .. module:upper()
  end
end

if config.modules.headset then
  for headset, enabled in pairs(config.headsets) do
    if enabled then
      cflags += '-DLOVR_USE_' .. headset:upper()
      src += 'src/modules/headset/headset_' .. headset .. '.c'
    end
  end
end

for renderer, enabled in pairs(config.renderers) do
  if enabled then
    local codes = { vulkan = 'vk', webgpu = 'webgpu' }
    local code = codes[renderer]
    cflags += '-DLOVR_' .. code:upper()
    src += 'src/core/gpu_' .. code .. '.c'
  end
end

for spatializer, enabled in pairs(config.spatializers) do
  if enabled then
    cflags += '-DLOVR_ENABLE_' .. spatializer:upper() .. '_SPATIALIZER'
    src += 'src/modules/audio/spatializer_' .. spatializer .. '.c'
  end
end

src += 'src/lib/stb/*.c'
src += (config.modules.audio or config.modules.data) and 'src/lib/miniaudio/*.c' or nil
src += config.modules.data and 'src/lib/jsmn/*.c' or nil
src += config.modules.data and 'src/lib/minimp3/*.c' or nil
src += config.modules.math and 'src/lib/noise1234/*.c' or nil
src += config.modules.thread and 'src/lib/tinycthread/*.c' or nil

res += 'src/resources/*.lua'
res += 'src/resources/*.ttf'
res += 'src/resources/*.json'

for i = 1, #res do
  src.extra_inputs += res[i] .. '.h'
end

src.extra_inputs += extra_inputs

obj += '.obj/*.o'
obj.extra_inputs = lib('*')

---> shaders

vert = 'src/resources/*.vert'
frag = 'src/resources/*.frag'
comp = 'src/resources/*.comp'

tup.foreach_rule(vert, 'glslangValidator --target-env vulkan1.1 --vn lovr_shader_%B_vert -o %o %f', '%f.h')
tup.foreach_rule(frag, 'glslangValidator --target-env vulkan1.1 --vn lovr_shader_%B_frag -o %o %f', '%f.h')
tup.foreach_rule(comp, 'glslangValidator --target-env vulkan1.1 --vn lovr_shader_%B -o %o %f', '%f.h')
tup.rule({ vert .. '.h', frag .. '.h', comp .. '.h' }, "cat %f | sed '/\\/\\//d' | sed '/#/d' > %o", 'src/resources/shaders.h')
src.extra_inputs += 'src/resources/shaders.h'

---> build

output = {({
  win32 = '$(bin)/lovr.exe',
  macos = '$(bin)/lovr',
  linux = '$(bin)/lovr',
  android = '$(bin)/liblovr.so',
  wasm = '$(bin)/lovr.html'
})[target]}

output.extra_outputs = extra_outputs

tup.foreach_rule(res, '^ XD %b^ xxd -i %f > %o', '%f.h')
tup.foreach_rule(src, '^ CC %b^ $(cc) $(base_flags) $(cflags) $(cflags_%B) -o %o -c %f', '.obj/%B.o')
tup.rule(obj, '^ LD %o^ $(cc) $(base_flags) -o %o %f $(lflags)', output)

---> apk

if target == 'android' then
  for _, key in pairs({ 'manifest', 'package', 'project' }) do
    local value = tup.getconfig('ANDROID_' .. key:upper())
    config.android[key] = #value > 0 and value or config.android[key]
  end

  activity =
    config.headsets.vrapi and 'src/resources/Activity_vrapi.java' or
    config.headsets.pico and 'src/resources/Activity_pico.java' or
    config.headsets.openxr and 'src/resources/Activity_openxr.java'

  java = 'bin/Activity.java'
  class = 'org/lovr/app/Activity.class'
  binclass = 'bin/' .. class
  jar = 'bin/lovr.jar'
  dex = 'bin/apk/classes.dex'

  androidversion = config.android.version
  androidjar = ('%s/platforms/android-%d/android.jar'):format(config.android.sdk, androidversion)
  extrajar = config.headsets.pico and 'deps/pico/classes.jar' or nil
  classpathsep = tup.getconfig('TUP_PLATFORM') == 'win32' and ';' or ':'
  classpath = table.concat({ androidjar, extrajar }, classpathsep)

  package = #config.android.package > 0 and ('--rename-manifest-package ' .. config.android.package) or ''
  project = #config.android.project > 0 and ('-A ' .. config.android.project) or ''

  manifest = config.android.manifest or
    config.headsets.vrapi and 'src/resources/AndroidManifest_oculus.xml' or
    config.headsets.pico and 'src/resources/AndroidManifest_pico.xml' or
    config.headsets.openxr and 'src/resources/AndroidManifest_oculus.xml'

  tools = config.android.sdk .. '/build-tools/' .. config.android.buildtools

  ks = config.android.keystore
  kspass = config.android.keystorepass

  unaligned = 'bin/.lovr.apk.unaligned'
  unsigned = 'bin/.lovr.apk.unsigned'
  apk = 'bin/lovr.apk'

  copy(manifest, 'bin/AndroidManifest.xml')
  copy(activity, java)
  tup.rule(java, '^ JAVAC %b^ javac -classpath $(classpath) -d bin %f', binclass)
  tup.rule(binclass, '^ JAR %b^ jar -cf %o -C bin $(class)', jar)
  tup.rule({ jar, extrajar }, '^ D8 %b^ $(tools)/d8 --min-api $(androidversion) --output bin/apk %f', dex)
  tup.rule(
    { 'bin/AndroidManifest.xml', extra_inputs = { lib('*'), dex } },
    '^ AAPT %b^ $(tools)/aapt package $(package) -F %o -M %f -0 so -I $(androidjar) $(project) bin/apk',
    unaligned
  )
  tup.rule(unaligned, '^ ZIPALIGN %f^ $(tools)/zipalign -f -p 4 %f %o', unsigned)
  tup.rule(unsigned, '^ APKSIGNER %o^ $(tools)/apksigner sign --ks $(ks) --ks-pass $(kspass) --out %o %f', { apk, extra_outputs = 'bin/lovr.apk.idsig' })
end
