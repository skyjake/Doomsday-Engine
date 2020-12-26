#!/usr/bin/env python3
import os, platform

def print_config(cfg):
    print("Configuration:")
    for key in cfg:
        print("  %-15s %s" % (key + ':', cfg[key]))


IS_MACOS  = platform.system() == 'Darwin'
IS_CYGWIN = platform.system().startswith('CYGWIN_NT')
IS_MSYS   = os.getenv('MSYSTEM') == 'MSYS'
IS_MINGW  = os.getenv('MSYSTEM') == 'MINGW64'

PATCH_DIR = os.path.abspath(os.path.dirname(__file__))

if IS_MSYS:
    UNISTRING_DIR = '-DUNISTRING_DIR=' + os.getenv('MSYSTEM_PREFIX')
elif IS_MINGW:
    UNISTRING_DIR = '-DUNISTRING_DIR=' + os.getenv('MINGW_PREFIX')
elif platform.system() == 'Darwin':
    UNISTRING_DIR = '-DUNISTRING_DIR=/usr/local'
else:
    UNISTRING_DIR = '' # installed on system

FORMATS = ['3DS', 'AC', 'ASE', 'ASSBIN', 'ASSXML', 'B3D', 'BVH', 'COLLADA',
           'DXF', 'CSM', 'HMP', 'IRR', 'LWO', 'LWS', 'MD2', 'MD3', 'MD5', 'MDC', 'MDL',
           'NFF', 'NDO', 'OFF', 'OBJ', 'OGRE', 'OPENGEX', 'PLY', 'MS3D', 'COB', 'BLEND',
           'IFC', 'XGL', 'FBX', 'Q3D', 'Q3BSP', 'RAW', 'SMD', 'STL', 'TERRAGEN', '3D', 'X',
           'AMF', 'IRRMESH', 'SIB', 'X3D', '3MF', 'MMD']

ENABLED_FORMATS = ['3DS', 'COLLADA', 'MD2', 'MD3', 'MD5', 'MDL', 'OBJ', 'BLEND', 'FBX', '3MF']

fmt_flag = {}
for fmt in FORMATS:
    fmt_flag[fmt] = 'YES' if fmt in ENABLED_FORMATS else 'NO'

# Builds dependencies using CMake.
# The libraries are installed under the specified directory.
dependencies = [
    (
        'the_Foundation',
        'https://git.skyjake.fi/skyjake/the_Foundation.git', 'origin/master',
        [UNISTRING_DIR,
         '-DTFDN_ENABLE_DEBUG_OUTPUT=YES',
         '-DTFDN_ENABLE_TLSREQUEST=NO']
    ),
    (
        'Open Asset Import Library',
        'https://github.com/assimp/assimp.git', 'v4.1.0',
        ['-Wno-dev',
         '-DASSIMP_BUILD_ASSIMP_TOOLS=OFF',
         '-DASSIMP_BUILD_TESTS=OFF'] +
        ['-DASSIMP_BUILD_%s_IMPORTER=%s' % (fmt, fmt_flag[fmt]) for fmt in FORMATS]
    ),
    (
        'cginternals/glbinding',
        'https://github.com/cginternals/glbinding.git', 'v2.1.4',
        ['-Wno-dev',
         '-DOPTION_BUILD_EXAMPLES=NO',
         '-DOPTION_BUILD_TOOLS=NO',
         '-DOPTION_BUILD_TESTS=NO',
         '-DCMAKE_CXX_FLAGS=-Wno-deprecated-copy' if IS_MSYS else '']
    )
]

if IS_MACOS: del dependencies[2] # using glbinding from Homebrew

import shutil
import subprocess
import sys
import json

CFG_PATH = os.path.join(os.path.expanduser('~'), '.doomsday', 'build_deps')

cfg = {
    'build_type': 'Release',
    'build_dir': os.path.abspath(
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', 'deps'
        )
    ),
    'generator': 'Unix Makefiles' if (IS_MSYS or IS_MINGW or IS_CYGWIN) else 'Ninja'
}
if os.path.exists(CFG_PATH):
    cfg = json.load(open(CFG_PATH, 'rt'))

show_help = (len(sys.argv) == 1)
do_build = False
do_clean = False
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
    elif opt == 'build':
        do_build = True
    elif opt == '-d':
        idx += 1
        cfg['build_dir'] = os.path.abspath(sys.argv[idx])
    else:
        raise Exception('Unknown option: ' + opt)
    idx += 1

if not do_build and show_help:
    print("""
Usage: build_deps.py [opts] [commands] build-dir

Commands:
  build           Build using existing config.
  clean           Clean the build directory.

Options:
  -G <generator>  Use CMake <generator> when configuring build.
  -t <type>       Set CMake build type (e.g., Release).
  -d <dir>        Set build directory.
  --help          Show this help.
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
if do_clean and os.path.exists(PRODUCTS_DIR):
    print("Deleting:", PRODUCTS_DIR)
    shutil.rmtree(PRODUCTS_DIR)
os.makedirs(PRODUCTS_DIR, exist_ok=True)

for long_name, git_url, git_tag, cmake_opts in dependencies:
    name = os.path.splitext(os.path.basename(git_url))[0]
    src_dir = os.path.join(BUILD_DIR, name)
    patch_file = os.path.join(PATCH_DIR, '%s-%s.patch' % (name, git_tag))
    if not os.path.exists(patch_file):
        patch_file = None
    if not os.path.exists(src_dir):
        os.makedirs(src_dir, exist_ok=True)
        os.chdir(src_dir)
        subprocess.check_call(['git', 'clone', git_url, '.'])
    else:
        os.chdir(src_dir)
        subprocess.check_call(['git', 'fetch', '--tags'])
    print('Current directory:', os.getcwd())
    subprocess.check_call(['git', 'reset', '--hard'])
    subprocess.check_call(['git', 'checkout', git_tag])
    if patch_file:
        print('Patching: %s' % os.path.basename(patch_file))
        subprocess.check_call(['patch', '-p1', '-i', patch_file])
    build_dir = os.path.join(src_dir, 'build')
    if do_clean and os.path.exists(build_dir):
        print("Deleting:", build_dir)
        shutil.rmtree(build_dir)
        continue
    if do_build:
        os.makedirs(build_dir, exist_ok=True)
        os.chdir(build_dir)
        print(build_dir)
        subprocess.check_call(['cmake',
                               '-G', GENERATOR,
                               '-DCMAKE_BUILD_TYPE=' + BUILD_TYPE,
                               '-DCMAKE_INSTALL_PREFIX=' + PRODUCTS_DIR] +
                              cmake_opts +
                              ['..'])
        subprocess.check_call(['cmake',
                               '--build', '.',
                               '--target', 'install'
        ])
