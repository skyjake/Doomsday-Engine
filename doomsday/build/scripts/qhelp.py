#!/usr/bin/env python
# Assumes qhelpgenerator is on the path (part of Qt)
import re, sys, os, string

print 'Patching index.qhp...'
input = file(sys.argv[1], 'rt').read()

# Doxygen writes a spurious </section> tag for some reason.
output = re.sub('</section>\s+<section title="Namespace Members"', '<!-- patched -->\n<section title="Namespace Members"', input)

# Rewrite the source file.
file(sys.argv[1], 'wt').write(output)

os.system('qhelpgenerator ' + string.join(sys.argv[1:], ' '))
