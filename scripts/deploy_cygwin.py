#!/usr/bin/python
# This script goes through a Cygwin deployment and copies all the required
# dependencies from the system directory. All executables (EXE, DLL) are
# checked.

from __future__ import print_function

import sys, os, shutil, subprocess, re, platform
if not platform.system().startswith('CYGWIN_'):
    raise Exception('Please run under cygwin')
system_bin = '/bin'
objdump_cmd = '/bin/objdump'
root = os.path.abspath(sys.argv[1])
if root.startswith('/usr'):
    print('cygwin_deps.py will not copy system libraries to a destination under /usr')
binaries = set(filter(lambda n: os.path.splitext(n)[1].lower() in ['.exe', '.dll'],
                      os.listdir(root)))
while binaries:
    binary = binaries.pop()
    if binary.startswith('lib'):
        # Not a cygwin library.
        continue
    print('--', binary)
    bin_path = os.path.join(root, binary)
    deps = subprocess.check_output([objdump_cmd, '-p', bin_path]).decode()
    for deps_line in deps.split('\n'):
        found = re.search('DLL Name: (.*)', deps_line)
        if found:
            dll_name = found.group(1).strip()
            dll_path = os.path.join(system_bin, dll_name)
            dest_path = os.path.join(root, dll_name)
            if os.path.exists(dll_path) and not os.path.exists(dest_path):
                # Copy this dependency.
                print(dll_path)
                shutil.copy(dll_path, dest_path)
                binaries.add(dest_path)
