#!/usr/bin/env hbpython
# Script for generating documentation for the wiki

import sys, os, subprocess
import build_version

sys.path += ['/Users/jaakko/Scripts']
import dew

is_dry_run = '--dry-run' in sys.argv
comment = 'Generated from Amethyst source by wikidocs.py'
build_version.find_version(quiet=True)

    
def amethyst(input, ame_opts=[]):
    """Runs amethyst with the given input and returns the output."""
    p = subprocess.Popen(['amethyst', '-dDOKUWIKI'] + ame_opts, 
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    (output, errs) = p.communicate(input)
    return output    

#
# for col in collections:
#     print 'Processing', col, 'documentation...'
#     pagesForCollection[col] = {}
#
#     for mode in modes:
#         print '-', mode
#
#         dirName = os.path.join(os.path.dirname(os.path.abspath(__file__)),
#                                '..', '..', 'doc', col, mode)
#         files = os.listdir(dirName)
#         for fn in files:
#             if not fn.endswith('.ame'): continue
#             name = fn[:-4]
#             title = name
#
#             if mode == 'command': title += " (Cmd)"
#
#             # Generate full page.
#             templ = '@require{amestd}\n'
#             templ += '@macro{summary}{== Summary == @break @arg}\n'
#             templ += '@macro{description}{@break == Description == @break @arg}\n'
#             templ += '@macro{cbr}{<br/>}\n'
#             templ += '@macro{usage}{@break === Usage === @break @arg}\n'
#             templ += '@macro{examples}{@break === Examples === @break @arg}\n'
#             templ += '@macro{seealso}{@break === See also === @break @arg}\n'
#             templ += '@macro{params}{@break === Usage === @break @arg}\n'
#             templ += '@macro{example}{@break === Example === @break @arg}\n'
#             templ += '@begin\n' + \
#                  file(os.path.join(dirName, fn)).read()
#
#             templ += '\n[[Category:%s]] @br\n' % categoryForMode(mode)
#             templ += '[[Category:%s]]\n' % categoryForCollection(col, mode)
#
#             page = Page(title, amethyst(templ))
#             page.mode = mode
#             pagesForCollection[col][title] = page
#
#             if isDryRun:
#                 print page.title
#                 print page.content
#
#             # Just the summary for the index.
#             templ = '@require{amestd}\n'
#             templ += '@macro{summary}{@arg}\n'
#             templ += '@macro{description}{}\n'
#             templ += '@macro{cbr}{}\n'
#             templ += '@begin\n' + \
#                  file(os.path.join(dirName, fn)).read()
#
#             page.name = name
#             page.summary = amethyst(templ).strip()

version = '%i.%i' % (build_version.DOOMSDAY_VERSION_MAJOR,
                     build_version.DOOMSDAY_VERSION_MINOR)
prefix = 'guide:%s' % version
src_root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../doomsday/doc')

pages = [
    ('%s:readme_windows' % prefix, ['-dWIN32'], 'readme/readme.ame'),
    ('%s:readme_macos' % prefix, ['-dMACOSX'], 'readme/readme.ame'),  
    ('%s:man:doomsday' % prefix, ['-dUNIX', '-dMANPAGE'], 'readme/readme.ame'), 
    ('%s:man:doomsday-server' % prefix, ['-dUNIX', '-dMANPAGE'], 'server/server.ame'),
    ('%s:man:doomsday-shell-text' % prefix, ['-dUNIX', '-dMANPAGE'], 'shell-text/shell-text.ame'),
]

print('Generating %i documents for Doomsday %s...' % (len(pages), version))

content = []
for page in pages:
    ame_path = os.path.join(src_root, page[2])
    ame_dir  = os.path.dirname(ame_path)
    src = open(ame_path, 'rt').read()
    opts = page[1] + ['-i'+ame_dir]
    content.append( (page[0], amethyst(src, ame_opts=opts)) )
    
                                        
if is_dry_run:
    print(content)
    sys.exit()   
    
        
dew.login()

print('Writing %i pages to Manual:' % len(content))
        
for c in content:
    print('... %s' % c[0])
    dew.submit_page(c[0], c[1], comment)
        
#dew.submitPage('Console command reference', indexPage['command'], comment)
#dew.submitPage('Console variable reference', indexPage['variable'], comment)
#
# print 'Submitting deambiguation pages...'
#
# for amb in ambigs:
#     print '-', amb.title
#     dew.submitPage(amb.title, amb.content, comment)
#
# print 'Submitting pages...'
#
# for col in collections:
#     pages = pagesForCollection[col]
#     count = len(pages)
#     i = 0
#     for page in pages.values():
#         i += 1
#         print '-', col, ": %i / %i" % (i, count)
#         dew.submitPage(page.title, page.content, comment)
#
dew.logout()

print('Done.')
