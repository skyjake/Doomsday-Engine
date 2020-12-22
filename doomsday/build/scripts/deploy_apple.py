# This script goes through the contents of an application bundle
# (both MacOS and Frameworks) and fixes the install names so that
# they use "@rpath" for all the dynamically linked libraries.

from __future__ import print_function

import sys, os, shutil, subprocess, re
ln_cmd = '/bin/ln'
chmod_cmd = '/bin/chmod'
otool_cmd = '/usr/bin/otool'
name_cmd = '/usr/bin/install_name_tool'
print("Fixing install names in:", sys.argv[1])
root = os.path.join(sys.argv[1], 'Contents')
binaries = \
    list(map(lambda n: '/MacOS/' + n, os.listdir(root + '/MacOS'))) + \
    list(map(lambda n: '/Frameworks/' + n, os.listdir(root + '/Frameworks')))
actions = {}
for binary in binaries:
    if binary.startswith('/Frameworks/'):
        subprocess.check_call([chmod_cmd, '0644', root + binary])
    # ver3 = re.match(r'(.*)\.((\d+)\.\d+\.\d+)\.(.*)', binary)
    # if ver3:
    #     print('version found:', binary, ver3.group(3))
    #     ver_symlink = root + '%s.%s.%s' % \
    #         (ver3.group(1), ver3.group(3), ver3.group(4))
    #     target = '%s.%s.%s' % (os.path.basename(ver3.group(1)), ver3.group(2), ver3.group(4))
    #     if not os.path.exists(ver_symlink):
    #         subprocess.check_call([ln_cmd, '-s', target, ver_symlink])
libraries = set()
while len(binaries) > 0:
    binary = binaries[0]
    del binaries[0]
    print(binary)
    if not re.search('cmd LC_RPATH', subprocess.check_output([otool_cmd, '-l', root + binary], encoding='utf-8')):
        subprocess.check_call([name_cmd, '-add_rpath', '@loader_path/../Frameworks', root + binary])
    deps = subprocess.check_output([otool_cmd, '-L', root + binary], encoding='utf-8')
    dep_actions = []
    for deps_line in deps.split('\n'):
        dep = re.match('\\s+(.*) \\(compatibility version.*', deps_line)
        if dep:
            dep_path = dep.group(1)
            dep_name = os.path.basename(dep_path)
            if dep_name.startswith('lib') and not dep_path.startswith('/usr/lib') \
                    and not dep_path.startswith('/System'):
                libraries.add(dep_name)
            if dep_path.startswith('@rpath') or dep_path.startswith('/System') or \
                    dep_path.startswith('/usr/lib'):
                # Not going to touch system libraries.
                continue
            dep_actions.append((dep_path, '@rpath/' + dep_name))
            new_dep_path = os.path.join(root, 'Frameworks', dep_name)
            if not os.path.exists(new_dep_path):
                print('%s: missing dependency!' % dep_name)
                shutil.copy(dep_path, new_dep_path)
                subprocess.check_call([chmod_cmd, '0644', new_dep_path])
                binaries.append('/Frameworks/' + dep_name)
    if dep_actions:
        actions[binary] = dep_actions
for binary in actions:
    print(binary)
    #print(actions[binary])
    path = root + binary
    if binary.startswith('/MacOS/'):
        subprocess.check_call([name_cmd, '-id', '@rpath/' + os.path.basename(binary), path])
    for old_path, new_path in actions[binary]:
        subprocess.check_call([name_cmd, '-change', old_path, new_path, path])
avail_fws = os.listdir(os.path.join(root, 'Frameworks'))
def base_name(fn):
    return fn[:fn.find('.')]
for lib in libraries:
    if not os.path.exists(os.path.join(root, 'Frameworks', lib)):
        print('%s: missing link' % lib)
        name = base_name(lib)
        for avail in avail_fws:
            if base_name(avail) == name:
                print(' -> to %s' % avail)
                subprocess.check_call([ln_cmd, '-s',
                    avail,
                    os.path.join(root, 'Frameworks', lib)])
                break
