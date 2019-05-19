#!/usr/bin/env python3

FORMATS = [ '3DS', 'AC', 'ASE', 'ASSBIN', 'ASSXML', 'B3D', 'BVH', 'COLLADA',
'DXF', 'CSM', 'HMP', 'IRR', 'LWO', 'LWS', 'MD2', 'MD3', 'MD5', 'MDC', 'MDL', 'NFF',
'NDO', 'OFF', 'OBJ', 'OGRE', 'OPENGEX', 'PLY', 'MS3D', 'COB', 'BLEND', 'IFC', 'XGL',
'FBX', 'Q3D', 'Q3BSP', 'RAW', 'SMD', 'STL', 'TERRAGEN', '3D', 'X']

ENABLED_FORMATS = ['3DS', 'COLLADA', 'MD2', 'MD3', 'MD5', 'MDL', 'OBJ', 'BLEND', 'FBX',
'IRR']

fmt_flag = {}
for fmt in FORMATS:
    fmt_flag[fmt] = 'YES' if fmt in ENABLED_FORMATS else 'NO'

# Builds dependencies using CMake.
# The libraries are installed under the specified directory.
dependencies = [
    (
        'the_Foundation',
        'ssh://git@github.com/skyjake/the_Foundation.git', 'origin/master',
        ['-DUNISTRING_DIR=/usr/local']
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
        'https://github.com/cginternals/glbinding.git', 'v3.0.2',
        ['-Wno-dev',
         '-DOPTION_BUILD_EXAMPLES=NO',
         '-DOPTION_BUILD_TOOLS=NO',
         '-DOPTION_BUILD_TESTS=NO']
    )
]

import os
import shutil
import subprocess
import sys

BUILD_TYPE = 'Release'
BUILD_DIR = os.path.normpath(os.path.abspath(sys.argv[1]))
GENERATOR = sys.argv[2]
PRODUCTS_DIR = os.path.join(BUILD_DIR, 'products')
os.makedirs(PRODUCTS_DIR, exist_ok=True)

for long_name, git_url, git_tag, cmake_opts in dependencies:
    name = os.path.splitext(os.path.basename(git_url))[0]
    src_dir = os.path.join(BUILD_DIR, name)
    if not os.path.exists(src_dir):
        os.makedirs(src_dir, exist_ok=True)
        os.chdir(src_dir)
        subprocess.check_call(['git', 'clone', git_url, '.'])
    else:
        os.chdir(src_dir)
        subprocess.check_call(['git', 'fetch', '--tags'])
    subprocess.check_call(['git', 'checkout', git_tag])
    build_dir = os.path.join(src_dir, 'build')
    if GENERATOR == 'clean':
        shutil.rmtree(build_dir)
        continue
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
