#!/usr/bin/env python3

import sys, codecs

BOM = u'\uFEFF'
includes = []
usings = set()
code = []

for fn in sys.argv[2:]:
    preamble = True
    in_body = False
    if_level = 0
    source = codecs.open(fn, 'r', 'utf-8').read()
    for line in source.split(u'\n'):
        if line.startswith(BOM):
            line = line[1:]
        original_line = line
        line = line.strip()
        if len(line) == 0: continue
        if line.startswith(u'#if'):
            if_level += 1
        elif line.startswith(u'#endif'):
            if_level -= 1
        if if_level == 0 and preamble and line[0] != u'/' and line[0] != u'#' and line[0] != u'*':
            preamble = False
        if preamble:
            includes.append(original_line)
        elif not in_body and line.startswith(u'using namespace'):
            usings.add(original_line)
        else:
            in_body = True
            code.append(original_line)
            
# Compress braces.
compressed = []
for i in range(len(code)):
    line = code[i].strip()
    if line == u'{' and len(compressed) and not compressed[-1].strip().startswith(u'#') and u'/*' not in compressed[-1] and u'*/' not in compressed[-1] and u'//' not in compressed[-1]:
        compressed[-1] += u' {'
    else:
        compressed.append(code[i])
code = compressed
    
out_file = codecs.open(sys.argv[1], 'w', 'utf-8')
out_file.write(BOM) # BOM
# Merged preamble.
out_file.write(u'\n'.join(includes) + u'\n')
out_file.write(u'\n'.join(usings) + u'\n')
# Merged source lines.
out_file.write(u'\n'.join(code) + u'\n')
out_file.close()
