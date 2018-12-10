# This script goes through the contents of an application bundle 
# (both MacOS and Frameworks) and fixes the install names so that
# they use "@rpath" for all the dynamically linked libraries.

from __future__ import print_function

import sys, os, shutil, subprocess, re
ln_cmd = '/bin/ln'
chmod_cmd = '/bin/chmod'
otool_cmd = '/usr/bin/otool'
name_cmd = '/usr/bin/install_name_tool'
root = os.path.join(sys.argv[1], 'Contents')
binaries = list(
    map(lambda n: '/MacOS/' + n, os.listdir(root + '/MacOS')) + \
    map(lambda n: '/Frameworks/' + n, os.listdir(root + '/Frameworks')))
actions = {}
for binary in binaries:
    ver3 = re.match(r'(.*)\.((\d+)\.\d+\.\d+)\.(.*)', binary)
    if ver3:
        print('version found:', binary, ver3.group(3))
        ver_symlink = root + '%s.%s.%s' % \
            (ver3.group(1), ver3.group(3), ver3.group(4))
        target = '%s.%s.%s' % (os.path.basename(ver3.group(1)), ver3.group(2), ver3.group(4))
        if not os.path.exists(ver_symlink):
            subprocess.call([ln_cmd, '-s', target, ver_symlink])
while len(binaries) > 0:        
    binary = binaries[0]
    del binaries[0]
    print(binary)
    if not re.search('cmd LC_RPATH', subprocess.check_output([otool_cmd, '-l', root + binary])):
        subprocess.call([name_cmd, '-add_rpath', '@loader_path/../Frameworks', root + binary])
    deps = subprocess.check_output([otool_cmd, '-L', root + binary])
    dep_actions = []
    for deps_line in deps.split('\n'):
        dep = re.match('\\s+(.*) \\(compatibility version.*', deps_line)
        if dep:
            dep_path = dep.group(1)
            if dep_path.startswith('@rpath') or dep_path.startswith('/System') or \
                    dep_path.startswith('/usr/lib'):
                # Not going to touch system libraries.
                continue
            dep_name = os.path.basename(dep_path)
            dep_actions.append((dep_path, '@rpath/' + dep_name))
            if not os.path.exists(os.path.join(root, 'Frameworks', dep_name)):
                print('%s: missing dependency!' % dep_name)
                shutil.copy(dep_path, os.path.join(root, 'Frameworks', dep_name))
                binaries.append('/Frameworks/' + dep_name)
    if dep_actions:
        actions[binary] = dep_actions
for binary in actions:
    print(binary)
    print(actions[binary])
    path = root + binary
    subprocess.call([name_cmd, '-id', '@rpath/' + os.path.basename(binary), path])
    for old_path, new_path in actions[binary]:
        subprocess.call([chmod_cmd, '0644', path])
        subprocess.call([name_cmd, '-change', old_path, new_path, path])
 