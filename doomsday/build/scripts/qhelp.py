#!/usr/bin/env python
# Assumes qhelpgenerator is on the path (part of Qt)
import re, sys, os, string

print 'Patching index.qhp...'
input = file(sys.argv[1], 'rt').read()

# Doxygen writes a spurious </section> tag for some reason.
#output = re.sub('</section>\s+<section title="Namespace Members"', '<!-- patched -->\n<section title="Namespace Members"', input)
output = re.sub('ref="index.html#Overview" />\n\s+</section>', 'ref="index.html#Overview" />\n<!-- patched -->', input)
output = re.sub('</section>\s+</section>\s+<section title="Math Utilities"', 
    '<!--patched--></section>\n<section title="Math Utilities"', output)
#output = re.sub('</section>\s+</toc>', '\n<!-- patched --></toc>', output)

# Rewrite the source file.
file(sys.argv[1], 'wt').write(output)

os.system('qhelpgenerator ' + string.join(sys.argv[1:], ' '))
