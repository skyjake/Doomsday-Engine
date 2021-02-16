#!/usr/bin/env python3
# This script goes through a MinGW deployment and copies all the required
# dependencies from the system directory. All executables (EXE, DLL) are
# checked.

from __future__ import print_function

import sys, os, shutil, subprocess, re
if os.getenv('MSYSTEM') != 'MINGW64':
    raise Exception('Please use mingw64 under msys2')
mingw_prefix = os.getenv('MINGW_PREFIX')
mingw_bin = mingw_prefix + '/bin'
objdump_cmd = mingw_bin + '/objdump'
root = os.path.join(sys.argv[1])
binaries = set(filter(lambda n: os.path.splitext(n)[1].lower() in ['.exe', '.dll'],
                      os.listdir(root)))
while binaries:
    binary = binaries.pop()
    print('--', binary)
    bin_path = os.path.join(root, binary)
    deps = subprocess.check_output([objdump_cmd, '-p', bin_path]).decode()
    for deps_line in deps.split('\n'):
        found = re.search('DLL Name: (.*)', deps_line)
        if found:
            dll_name = found.group(1).strip()
            dll_path = os.path.join(mingw_bin, dll_name)
            dest_path = os.path.join(root, dll_name)
            if os.path.exists(dll_path) and not os.path.exists(dest_path):
                # Copy this dependency.
                print(dll_path)
                shutil.copy(dll_path, dest_path)
                binaries.add(dest_path)
