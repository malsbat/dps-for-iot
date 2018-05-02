import os
import platform

vars = Variables()

bindings = Split('python nodejs')

# Generic build variables
vars.AddVariables(
    EnumVariable('variant', 'Build variant', default='release', allowed_values=('debug', 'release'), ignorecase=2),
    EnumVariable('transport', 'Transport protocol', default='udp', allowed_values=('udp', 'tcp', 'dtls', 'fuzzer'), ignorecase=2),
    EnumVariable('target', 'Build target', default='local', allowed_values=('local', 'yocto'), ignorecase=2),
    ListVariable('bindings', 'Bindings to build', bindings, bindings),
    ('CC', 'C compiler to use'),
    ('CXX', 'C++ compiler to use'))

# Windows-specific command line variables
if platform.system() == 'Windows':
    vars.AddVariables(
        PathVariable('UV_PATH', 'Path where libuv is installed', 'ext\libuv', PathVariable.PathAccept),
        PathVariable('PYTHON_PATH', 'Path to Python', 'C:\Python27', PathVariable.PathAccept),
        PathVariable('SWIG', 'Path to SWIG executable', 'C:\swigwin-3.0.10\swig.exe', PathVariable.PathAccept),
        PathVariable('DOXYGEN_PATH', 'Path to Doxygen', 'C:\Program Files\Doxygen', PathVariable.PathAccept))

# Linux-specific command line variables
if platform.system() == 'Linux':
    vars.AddVariables(
        PathVariable('UV_PATH', 'Path where libuv is installed', '', PathVariable.PathAccept),
        BoolVariable('profile', 'Build for profiling?', False),
        BoolVariable('asan', 'Enable address sanitizer?', False),
        BoolVariable('tsan', 'Enable thread sanitizer?', False),
        BoolVariable('ubsan', 'Enable undefined behavior sanitizer?', False),
        BoolVariable('fsan', 'Enable fuzzer sanitizer?', False),
        BoolVariable('cov', 'Enable code coverage?', False))

tools=['default', 'textfile', DPS]
# Doxygen is optional
try:
    env = Environment(tools=['doxygen'])
    tools.append('doxygen')
except:
    pass

extEnv = Environment(ENV = os.environ, variables=vars)

env = Environment(
    CPPPATH=[
        '#/inc',
    ],
    CPPDEFINES=[
    ],
    variables=vars,
    tools=tools,
)

Help(vars.GenerateHelpText(env))

for key, val in ARGLIST:
    if key.lower() == 'define':
        env['CPPDEFINES'].append(val)

# Unpack bindings into individually testable booleans
for b in bindings:
    env[b] = b in env['bindings']

if env['transport'] == 'udp':
    env['USE_UDP'] = 'true'
    env['CPPDEFINES'].append('DPS_USE_UDP')

print("Building for " + env['variant'])

# Platform specific configuration

if env['PLATFORM'] == 'win32':

    env.Append(CCFLAGS = ['/J', '/W3', '/WX', '/nologo'])

    if env['variant'] == 'debug':
        env.Append(CCFLAGS = ['/Zi', '/MT', '/Od', '-DDPS_DEBUG'])
        env.Append(LINKFLAGS = ['/DEBUG'])
    else:
        env.Append(CCFLAGS = ['/Gy', '/O2', '/GF', '/MT'])
        env.Append(LINKFLAGS = ['/opt:ref'])

    # Stack-based Buffer Overrun Detection
    env.Append(CCFLAGS = ['/GS'])
    # Compiler settings validation
    env.Append(CCFLAGS = ['/sdl'])

    # Data Execution Prevention
    env.Append(LINKFLAGS = ['/NXCompat'])
    # Image Randomization
    env.Append(LINKFLAGS = ['/DynamicBase'])

    # Where to find Python.h
    env['PY_CPPPATH'] = [env['PYTHON_PATH'] + '\include']
    env['PY_LIBPATH'] = [env['PYTHON_PATH'] + '\libs']

    # Where to find libuv and the libraries it needs
    env['UV_LIBS'] = ['ws2_32', 'psapi', 'iphlpapi', 'shell32', 'userenv', 'user32', 'advapi32']
    if extEnv['UV_PATH'] != os.path.join('ext', 'libuv'):
        env['UV_LIBS'] = ['libuv'] + env['UV_LIBS']
    env.Append(LIBPATH=[env['UV_PATH']])
    env.Append(CPPPATH=[env['UV_PATH'] + '\include'])

    # Doxygen needs to be added to default path if available
    if env['DOXYGEN_PATH']:
        env.PrependENVPath('PATH', env['DOXYGEN_PATH'] + '/bin')

elif env['PLATFORM'] == 'posix':

    # Enable address sanitizer
    if env['asan'] == True:
        env.Append(CCFLAGS = ['-fno-omit-frame-pointer', '-fsanitize=address'])
        if env['CC'].endswith('gcc'):
            env.Append(LIBS = ['asan'])
        elif env['CC'].endswith('clang'):
            env.Append(LINKFLAGS = ['-fsanitize=address'])
        else:
            print('Unsupported compiler')
            exit();

    # Enable thread sanitizer
    if env['tsan'] == True:
        env.Append(CCFLAGS = ['-fsanitize=thread'])
        if env['CC'].endswith('gcc'):
            env.Append(LIBS = ['tsan'])
        elif env['CC'].endswith('clang'):
            env.Append(LINKFLAGS = ['-fsanitize=thread'])
        else:
            print('Unsupported compiler')
            exit();

    # Enable undefined behavior sanitizer
    if env['ubsan'] == True:
        env.Append(CCFLAGS = ['-fsanitize=undefined'])
        if env['CC'].endswith('gcc'):
            env.Append(LIBS = ['ubsan'])
        elif env['CC'].endswith('clang'):
            env.Append(LINKFLAGS = ['-fsanitize=undefined'])
        else:
            print('Unsupported compiler')
            exit();

    # Enable fuzzer sanitizer
    if env['fsan'] == True:
        if env['CC'].endswith('clang'):
            env.Append(CCFLAGS = ['-fsanitize=fuzzer-no-link'])
        else:
            print('Unsupported compiler')
            exit();

    # Enable code coverage
    if env['cov'] == True:
        if env['CC'].endswith('clang'):
            env.Append(CCFLAGS = ['-fprofile-instr-generate', '-fcoverage-mapping'])
            env.Append(LINKFLAGS = ['-fprofile-instr-generate', '-fcoverage-mapping'])
        else:
            print('Unsupported compiler')
            exit();

    # Stack execution protection:
    env.Append(LINKFLAGS = ['-z', 'noexecstack'])

    # Data relocation and protection (RELRO):
    env.Append(LINKFLAGS = ['-z', 'relro', '-z', 'now'])

    # Stack-based Buffer Overrun Detection:
    env.Append(CCFLAGS = ['-fstack-protector-strong'])

    # Position Independent Execution (PIE)
    env.Append(CCFLAGS = ['-fPIE', '-fPIC'])
    env.Append(LINKFLAGS = ['-pie']) # PIE for executables only

    # Fortify source:
    env.Append(CPPDEFINES = ['_FORTIFY_SOURCE=2'])

    # Format string vulnerabilities
    env.Append(CCFLAGS= ['-Wformat', '-Wformat-security'])

    # gcc option  -mmsse4.2 is to enble generation of popcountq instruction
    env.Append(CCFLAGS = ['-ggdb', '-msse4.2'])

    # Treat warnings as errors
    env.Append(CCFLAGS = ['-Werror'])

    if env['profile'] == True:
        env.Append(CCFLAGS = ['-pg'])
        env.Append(LINKFLAGS = ['-pg'])

    if env['variant'] == 'debug':
        env.Append(CCFLAGS = ['-O', '-DDPS_DEBUG'])
    else:
        env.Append(CCFLAGS = ['-O3', '-DNDEBUG'])

    # Where to find Python.h
    if env['target'] == 'yocto':
        env['PY_CPPPATH'] = [os.getenv('SYSROOT') + '/usr/include/python2.7']
    else:
        env['PY_CPPPATH'] = ['/usr/include/python2.7']
    env['PY_LIBPATH'] = []

    # Where to find libuv and the libraries it needs
    env['UV_LIBS'] = ['uv', 'pthread']

    if env['UV_PATH']:
        env.Prepend(LIBPATH = env['UV_PATH'])
        env.Prepend(CPPPATH = env['UV_PATH'] + '/include')

else:
    print('Unsupported system')
    exit()

env.Append(LIBPATH=['./ext'])

print(env['CPPDEFINES'])

if env['target'] == 'yocto':
    env_options = ["SYSROOT", "CC", "AR", "ARFLAGS", "CCFLAGS", "CFLAGS", "CXX", "CXXFLAGS", "LINKFLAGS", "STRIP", "PKG_CONFIG", "CHRPATH", "LD", "TAR"]
    for i in env_options:
        if os.environ.has_key(i):
            if i in ("CFLAGS", "CCFLAGS", "LINKFLAGS"):
                env.Replace(**{i: Split(os.getenv(i))})
                extEnv.Replace(**{i: Split(os.getenv(i))})
            else:
                env.Replace(**{i: os.getenv(i)})
                extEnv.Replace(**{i: os.getenv(i)})
    env.PrependENVPath('PATH', os.getenv('PATH'))
    env.PrependENVPath('LDFLAGS', os.getenv('LDFLAGS'))
    extEnv.PrependENVPath('PATH', os.getenv('PATH'))
    extEnv.PrependENVPath('LDFLAGS', os.getenv('LDFLAGS'))

# Build external dependencies
ext_libs = SConscript('ext/SConscript', exports=['extEnv'])

version = '0.9.0'

SConscript('SConscript', src_dir='.', variant_dir='build/obj', duplicate=0, exports=['env', 'ext_libs', 'version'])

######################################################################
# Scons to generate the dps_ns3.pc file from dps_ns3.pc.in file
######################################################################
pc_file = 'dps_ns3.pc.in'
pc_vars = {'\@PREFIX\@': env.GetLaunchDir().encode('string_escape'),
           '\@VERSION\@': version,
}
env.Substfile(pc_file, SUBST_DICT = pc_vars)
