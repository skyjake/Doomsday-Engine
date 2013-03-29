# Reads a cphelp/conhelp file and outputs an .ame source for each
# command and variable.
#
# Arguments:
# 1 - input file (cphelp/conhelp)
# 2 - component (engine, libcommon, etc.)

import os
import sys

def escaped(s):
    result = ''
    for c in s:
        if c == '{': c = '@{'
        elif c == '}': c = '@}'
        elif c == '@': c = '@@'
        result += c
    return result

source = sys.argv[1]
component = sys.argv[2]
mode = None
name = ''
path = None
output = None

print 'Component:', component

for line in file(source).readlines():
    line = line.strip()
    if len(line) == 0: continue
    
    if '# CONSOLE VARIABLE' in line: mode = 'variable'
    elif '# CONSOLE COMMAND' in line: mode = 'command'
    
    if mode is None: continue
    
    if line.startswith('['):
        name = line[1:-1].strip()        
        outDir = os.path.join(component, mode)
        path = os.path.join(outDir, name + '.ame')
        print path
        if not os.path.exists(outDir): os.makedirs(outDir)
        output = file(path, 'w')
        
    if output != None:
        if line.startswith('desc'):
            desc = line[line.index('=') + 1:].strip()
            print >> output, "@summary{\n    %s\n}" % escaped(desc)
        elif line.startswith('inf'):
            info = line[line.index('=') + 1:].strip()
            print >> output, "@description{\n    %s\n}" % escaped(info)
    