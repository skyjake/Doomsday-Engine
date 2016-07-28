#!/usr/bin/env python

import sys, codecs, platform

BOM = u'\uFEFF'
includes = []
usings = set()
code = []

# Read and process all input source files.
for fn in sys.argv[2:]:
    preamble = True
    in_body = False
    if_level = 0
    source = codecs.open(fn, 'r', 'utf-8').read()
    for line in source.split(u'\n'):
        # Strip BOMs.
        if line.startswith(BOM):
            line = line[1:]
        original_line = line.rstrip()
        line = line.strip()
        # Ignore empty lines and single-line comments.
        if len(line) == 0: continue
        if line.startswith(u'//'): continue
        # Keep track of define blocks, they will be included as-is.
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

if platform.system() == 'Windows':
    # All kinds of wacky useless defines.
    includes.append(u'#undef min')
    includes.append(u'#undef max')
    includes.append(u'#undef small')
    includes.append(u'#undef SearchPath')

# Compress braces.
compressed = []
for i in range(len(code)):
    line = code[i].strip()
    if line == u'{' and len(compressed) and not compressed[-1].strip().startswith(u'#') and u'/*' not in compressed[-1] and u'*/' not in compressed[-1] and u'//' not in compressed[-1]:
        compressed[-1] += u' {'
    else:
        compressed.append(code[i])
code = compressed

NL = u'\n'

# Write the merged source file.
out_file = codecs.open(sys.argv[1], 'w', 'utf-8')
out_file.write(BOM) # BOM
# Merged preamble.
out_file.write(NL.join(includes) + NL)
out_file.write(NL.join(usings) + NL)
# Merged source lines.
out_file.write(NL.join(code) + NL)
out_file.close()
