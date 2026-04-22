#!/usr/bin/env python3
# build_deps.py -- External dependency builder for Doomsday Engine.
#
# Edit the values below to bump dependency versions or change git sources.
# Everything after the divider is build machinery that should not normally need
# to be modified.

THE_FOUNDATION_GIT = 'https://git.skyjake.fi/skyjake/the_Foundation.git'
THE_FOUNDATION_TAG = 'origin/main'

SDL3_VERSION       = '3.4.4'
SDL3_MIXER_VERSION = '3.2.0'

FLUIDSYNTH_VERSION = '2.5.4'

ASSIMP_GIT = 'https://github.com/assimp/assimp.git'
ASSIMP_TAG = 'v6.0.4'
ASSIMP_ENABLED_IMPORTERS = [
    '3DS', 'COLLADA', 'MD2', 'MD3', 'MD5', 'MDL', 'OBJ', 'BLEND', 'FBX', '3MF'
]

GLBINDING_GIT = 'https://github.com/cginternals/glbinding.git'
GLBINDING_TAG = 'v3.5.0'

#----------------------------------------------------------------------------------------

import os, platform, subprocess, shutil, sys, json, urllib.request, zipfile, tempfile

IS_MACOS   = platform.system() == 'Darwin'
IS_WINDOWS = platform.system() == 'Windows'
IS_CYGWIN  = platform.system().startswith('CYGWIN_NT')
IS_MSYS    = os.getenv('MSYSTEM') == 'MSYS'
IS_MINGW   = os.getenv('MSYSTEM') == 'MINGW64' and os.getenv('MINGW_PREFIX') is not None
# Native Windows/MSVC: not running inside an MSYS2/Cygwin shell.
IS_NATIVE_WINDOWS = IS_WINDOWS and not IS_MSYS and not IS_MINGW and not IS_CYGWIN

build_foundation = build_assimp = build_glbinding = True
build_sdl3 = build_sdl3_mixer = True
build_fluidsynth = IS_NATIVE_WINDOWS

PATCH_DIR = os.path.abspath(os.path.dirname(__file__))

if IS_MSYS:
    UNISTRING_DIR = '-DUNISTRING_DIR=' + os.getenv('MSYSTEM_PREFIX')
elif IS_MINGW:
    UNISTRING_DIR = '-DUNISTRING_DIR=' + os.getenv('MINGW_PREFIX')
elif IS_MACOS:
    UNISTRING_DIR = '-DUNISTRING_DIR=' + ('/opt/homebrew' if os.path.exists('/opt/homebrew') else '/usr/local')
else:
    UNISTRING_DIR = ''  # installed on system

# Complete list of all assimp importers; each is enabled or disabled via ASSIMP_ENABLED_IMPORTERS.
ASSIMP_ALL_IMPORTERS = [
    '3DS', 'AC', 'ASE', 'ASSBIN', 'ASSXML', 'B3D', 'BVH', 'COLLADA',
    'DXF', 'CSM', 'HMP', 'IRR', 'LWO', 'LWS', 'MD2', 'MD3', 'MD5', 'MDC', 'MDL',
    'NFF', 'NDO', 'OFF', 'OBJ', 'OGRE', 'OPENGEX', 'PLY', 'MS3D', 'COB', 'BLEND',
    'IFC', 'XGL', 'FBX', 'Q3D', 'Q3BSP', 'RAW', 'SMD', 'STL', 'TERRAGEN', '3D', 'X',
    'AMF', 'IRRMESH', 'SIB', 'X3D', '3MF', 'MMD',
]
_assimp_fmt_flag = {fmt: ('YES' if fmt in ASSIMP_ENABLED_IMPORTERS else 'NO')
                   for fmt in ASSIMP_ALL_IMPORTERS}

# Builds dependencies using CMake.
# The libraries are installed under the specified directory.
dependencies = [
    (
        'the_Foundation',
        THE_FOUNDATION_GIT, THE_FOUNDATION_TAG,
        ([UNISTRING_DIR] if UNISTRING_DIR else []) + [
            '-DTFDN_ENABLE_DEBUG_OUTPUT=YES',
            '-DTFDN_ENABLE_TLSREQUEST=NO',
            '-DTFDN_ENABLE_TESTS=NO',
            '-DTFDN_ENABLE_WIN32_FILE_API=' + ('YES' if IS_WINDOWS or IS_MSYS or IS_MINGW else 'NO'),
        ]
    ),
    (
        'Open Asset Import Library',
        ASSIMP_GIT, ASSIMP_TAG,
        ['-Wno-dev',
         '-DASSIMP_BUILD_ASSIMP_TOOLS=OFF',
         '-DASSIMP_BUILD_TESTS=OFF'] +
        (['-DCMAKE_OSX_SYSROOT=' + subprocess.check_output(
              ['xcrun', '--show-sdk-path']).decode().strip()] if IS_MACOS else []) +
        ['-DASSIMP_BUILD_%s_IMPORTER=%s' % (fmt, _assimp_fmt_flag[fmt])
         for fmt in ASSIMP_ALL_IMPORTERS]
    ),
    (
        'cginternals/glbinding',
        GLBINDING_GIT, GLBINDING_TAG,
        ['-Wno-dev',
         '-DOPTION_BUILD_EXAMPLES=NO',
         '-DOPTION_BUILD_TOOLS=NO',
         '-DOPTION_BUILD_TESTS=NO'] +
        (['-DCMAKE_CXX_FLAGS=-Wno-deprecated-copy'] if IS_MSYS or IS_MINGW else [])
    ),
]


def print_config(cfg):
    print("Configuration:")
    for key in cfg:
        print("  %-15s %s" % (key + ':', cfg[key]))


def download_prebuilt(url, products_dir):
    """Download a prebuilt zip release and merge its install tree into products_dir."""
    with tempfile.TemporaryDirectory() as tmp:
        zip_path = os.path.join(tmp, 'download.zip')
        print('  Downloading %s ...' % os.path.basename(url))
        urllib.request.urlretrieve(url, zip_path)
        extract_dir = os.path.join(tmp, 'extracted')
        os.makedirs(extract_dir)
        with zipfile.ZipFile(zip_path) as zf:
            zf.extractall(extract_dir)
        # If the zip has a single top-level directory, descend into it.
        entries = os.listdir(extract_dir)
        src_root = (os.path.join(extract_dir, entries[0])
                    if len(entries) == 1 and os.path.isdir(os.path.join(extract_dir, entries[0]))
                    else extract_dir)
        for item in os.listdir(src_root):
            src = os.path.join(src_root, item)
            dst = os.path.join(products_dir, item)
            if os.path.isdir(src):
                shutil.copytree(src, dst, dirs_exist_ok=True)
            else:
                shutil.copy2(src, dst)
    print('  Installed prebuilt package to %s' % products_dir)


def download_dmg(url, products_dir):
    """Download a macOS .dmg, mount it, merge its contents into products_dir, then unmount."""
    with tempfile.TemporaryDirectory() as tmp:
        dmg_path = os.path.join(tmp, 'download.dmg')
        mount_point = os.path.join(tmp, 'mount')
        os.makedirs(mount_point)
        print('  Downloading %s ...' % os.path.basename(url))
        urllib.request.urlretrieve(url, dmg_path)
        subprocess.check_call(['hdiutil', 'attach', '-quiet', '-noautoopen',
                               dmg_path, '-mountpoint', mount_point])
        try:
            for item in os.listdir(mount_point):
                if item.startswith('.'):
                    continue
                src = os.path.join(mount_point, item)
                dst = os.path.join(products_dir, item)
                if os.path.isdir(src):
                    shutil.copytree(src, dst, dirs_exist_ok=True, symlinks=True)
                else:
                    shutil.copy2(src, dst)
        finally:
            subprocess.call(['hdiutil', 'detach', mount_point, '-quiet'])
    print('  Installed prebuilt package to %s' % products_dir)


def sdl3_url(version):
    tag = 'release-' + version
    if IS_NATIVE_WINDOWS:
        return ('https://github.com/libsdl-org/SDL/releases/download/'
                '%s/SDL3-devel-%s-VC.zip' % (tag, version))
    elif IS_MACOS:
        return ('https://github.com/libsdl-org/SDL/releases/download/'
                '%s/SDL3-%s.dmg' % (tag, version))
    return None


def sdl3_mixer_url(version):
    tag = 'release-' + version
    if IS_NATIVE_WINDOWS:
        return ('https://github.com/libsdl-org/SDL_mixer/releases/download/'
                '%s/SDL3_mixer-devel-%s-VC.zip' % (tag, version))
    elif IS_MACOS:
        return ('https://github.com/libsdl-org/SDL_mixer/releases/download/'
                '%s/SDL3_mixer-%s.dmg' % (tag, version))
    return None


def fluidsynth_url(version):
    tag = 'v' + version
    if IS_NATIVE_WINDOWS:
        return ('https://github.com/FluidSynth/fluidsynth/releases/download/'
                '%s/fluidsynth-v%s-win10-x64-cpp11.zip' % (tag, version))
    return None


def install_sdl_package(name, version, url, products_dir):
    """Download and install an SDL prebuilt package, skipping if already at this version."""
    sentinel = os.path.join(products_dir, '.%s-version' % name.lower().replace('_', '-'))
    if os.path.exists(sentinel):
        with open(sentinel) as f:
            if f.read().strip() == version:
                print('  %s %s already installed.' % (name, version))
                return
    if url.endswith('.dmg'):
        download_dmg(url, products_dir)
    else:
        download_prebuilt(url, products_dir)
    with open(sentinel, 'w') as f:
        f.write(version)


def install_fluidsynth_package(version, products_dir):
    """Download and install FluidSynth as a prebuilt package, skipping if already 
    at this version."""
    sentinel = os.path.join(products_dir, '.fluidsynth-version')
    if os.path.exists(sentinel):
        with open(sentinel) as f:
            if f.read().strip() == version:
                print('  FluidSynth %s already installed.' % version)
                return
    download_prebuilt(fluidsynth_url(version), products_dir)
    with open(sentinel, 'w') as f:
        f.write(version)


CFG_PATH = os.path.join(os.path.expanduser('~'), '.doomsday', 'build_deps')

cfg = {
    'build_type': 'Release',
    'build_dir': os.path.abspath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'deps')
    ),
    'generator': ('Unix Makefiles' if (IS_MSYS or IS_MINGW or IS_CYGWIN) else
                  'Visual Studio 18 2026' if IS_WINDOWS else
                  'Ninja'),
}
if os.path.exists(CFG_PATH):
    cfg = json.load(open(CFG_PATH, 'rt'))

show_help = (len(sys.argv) == 1)
do_build = False
do_clean = False
do_distclean = False
idx = 1
while idx < len(sys.argv):
    opt = sys.argv[idx]
    if opt == '-G':
        idx += 1
        cfg['generator'] = sys.argv[idx]
    elif opt == '-t':
        idx += 1
        cfg['build_type'] = sys.argv[idx]
    elif opt == '--help':
        show_help = True
    elif opt == 'clean':
        do_clean = True
    elif opt == 'distclean':
        do_clean = True
        do_distclean = True
    elif opt == 'build':
        do_build = True
    elif opt == '-d':
        idx += 1
        cfg['build_dir'] = os.path.abspath(sys.argv[idx])
    elif opt == '--skip-foundation':
        build_foundation = False
    elif opt == '--skip-assimp':
        build_assimp = False
    elif opt == '--skip-glbinding':
        build_glbinding = False
    elif opt == '--skip-sdl3':
        build_sdl3 = False
    elif opt == '--skip-sdl3-mixer':
        build_sdl3_mixer = False
    elif opt == '--skip-fluidsynth':
        build_fluidsynth = False
    else:
        raise Exception('Unknown option: ' + opt)
    idx += 1

if not do_build and show_help:
    print("""
Usage: build_deps.py [opts] [commands]

Commands:
  build         Build using existing config.
  clean         Delete build directories (keeps installed products).
  distclean     Delete both build and the installed products directories.

Options:
  -G <generator>      Use CMake <generator> when configuring build.
  -t <type>           Set CMake build type (e.g., Release).
  -d <dir>            Set build directory.
  --help              Show this help.
  --skip-foundation   Do not build the_Foundation library
  --skip-assimp       Do not build Open Asset Importer library
  --skip-glbinding    Do not build glbinding library
  --skip-sdl3         Do not download SDL3 (Windows/macOS only)
  --skip-sdl3-mixer   Do not download SDL3_mixer (Windows/macOS only)
  --skip-fluidsynth   Do not download FluidSynth (Windows only)
""")
    print_config(cfg)
    exit(0)

if not os.path.exists(os.path.dirname(CFG_PATH)):
    os.mkdir(os.path.dirname(CFG_PATH))

print_config(cfg)
json.dump(cfg, open(CFG_PATH, 'wt'))

BUILD_TYPE = cfg['build_type']
BUILD_DIR = cfg['build_dir']
GENERATOR = cfg['generator']
PRODUCTS_DIR = os.path.join(BUILD_DIR, 'products')
if do_distclean and os.path.exists(PRODUCTS_DIR):
    print("Deleting:", PRODUCTS_DIR)
    shutil.rmtree(PRODUCTS_DIR)
os.makedirs(PRODUCTS_DIR, exist_ok=True)

if not build_glbinding:
    del dependencies[2]
if not build_assimp:
    del dependencies[1]
if not build_foundation:
    del dependencies[0]

IS_VS_GENERATOR = 'Visual Studio' in GENERATOR

for long_name, git_url, git_tag, cmake_opts in dependencies:
    name = os.path.splitext(os.path.basename(git_url))[0]
    print('\n--- %s ---' % long_name)

    src_dir = os.path.join(BUILD_DIR, name)
    patch_file = os.path.join(PATCH_DIR, '%s-%s.patch' % (name, git_tag))
    if not os.path.exists(patch_file):
        patch_file = None
    # Strip remote prefix so "origin/main" becomes "main" for --branch.
    clone_branch = git_tag.split('/')[-1] if '/' in git_tag else git_tag
    if not os.path.exists(src_dir):
        os.makedirs(src_dir, exist_ok=True)
        os.chdir(src_dir)
        subprocess.check_call(['git', 'clone', '--depth', '1',
                               '--branch', clone_branch, git_url, '.'])
    else:
        os.chdir(src_dir)
        subprocess.check_call(['git', 'fetch', '--depth', '1', '--tags'])
    print('Current directory:', os.getcwd())
    subprocess.check_call(['git', 'reset', '--hard'])
    subprocess.check_call(['git', 'checkout', git_tag])
    if patch_file:
        print('Patching: %s' % os.path.basename(patch_file))
        subprocess.check_call(['patch', '-p1', '-i', patch_file])
    build_dir = os.path.join(src_dir, 'build')
    revision_file = os.path.join(build_dir, '.built-revision')
    if do_clean and os.path.exists(build_dir):
        print("Deleting:", build_dir)
        shutil.rmtree(build_dir)
        continue
    if do_build:
        # Determine whether we need a full reconfigure (new revision or no prior build).
        current_rev = subprocess.check_output(
            ['git', 'rev-parse', 'HEAD'], cwd=src_dir).decode().strip()
        needs_configure = True
        if os.path.exists(revision_file):
            with open(revision_file) as f:
                built_rev = f.read().strip()
            if built_rev == current_rev:
                print('Revision unchanged (%s), skipping reconfigure.' % current_rev[:12])
                needs_configure = False
            else:
                print('Revision changed (%s -> %s), rebuilding from scratch.' %
                      (built_rev[:12], current_rev[:12]))
                shutil.rmtree(build_dir)
        os.makedirs(build_dir, exist_ok=True)
        os.chdir(build_dir)
        if needs_configure:
            cmake_configure = (['cmake', '-G', GENERATOR] +
                               (['-A', 'x64'] if IS_VS_GENERATOR else []) +
                               ['-DCMAKE_BUILD_TYPE=' + BUILD_TYPE,
                                '-DCMAKE_INSTALL_PREFIX=' + PRODUCTS_DIR] +
                               cmake_opts + ['..'])
            subprocess.check_call(cmake_configure)
        cmake_build = (['cmake', '--build', '.',
                        '--config', BUILD_TYPE,
                        '--target', 'install'] +
                       # Limit MSBuild parallelism to avoid D8040 resource errors
                       # on Windows when many source files compile simultaneously.
                       (['--', '/maxcpucount:4'] if IS_VS_GENERATOR else []))
        subprocess.check_call(cmake_build)
        with open(revision_file, 'w') as f:
            f.write(current_rev)

# SDL3 and SDL3_mixer: prebuilt binaries on Windows/macOS; system packages on Linux.
for name, version, url_fn, enabled in [
        ('SDL3',       SDL3_VERSION,       sdl3_url,       build_sdl3),
        ('SDL3_mixer', SDL3_MIXER_VERSION, sdl3_mixer_url, build_sdl3_mixer)]:
    if not enabled:
        continue
    print('\n--- %s ---' % name)
    url = url_fn(version)
    if url is None:
        print('  Linux: assuming system-installed %s.' % name)
        continue
    if do_build:
        install_sdl_package(name, version, url, PRODUCTS_DIR)

# FluidSynth: prebuilt binaries on Windows.
if build_fluidsynth:
    print('\n--- FluidSynth ---')
    if do_build:
        install_fluidsynth_package(FLUIDSYNTH_VERSION, PRODUCTS_DIR)

print()
