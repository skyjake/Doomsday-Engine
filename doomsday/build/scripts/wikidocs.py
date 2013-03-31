import sys, os, subprocess

sys.path += ['/Users/jaakko/Dropbox/Scripts']
import dew

comment     = 'Generated from Amethyst source by wikidocs.py'
collections = ['engine', 'libcommon', 'libdoom', 'libheretic', 'libhexen']
modes       = ['command', 'variable']


def categoryForMode(m):
    """Wiki category for the mode."""
    return {'command':  'Console command', 
            'variable': 'Console variable'}[m]

def categoryForCollection(col, m):
    """Wiki category for a collection."""
    if col == 'engine': col = 'Engine'
    return categoryForMode(m) + ' (%s)' % col
    
def heading(col, m):
    """Heading on the index page for a particular collection."""
    hd = {'command': 'Commands', 'variable': 'Variables'}[m]
    if col == 'engine': col = 'Doomsday'
    elif col == 'libcommon': col = 'all games'
    hd += ' for '  + col
    return hd

def colWidth(m):
    """Width of an index table column."""
    if m == 'variable': return 33
    return 20

def colHeading(m):
    """Index table heading."""
    hd = {'command': 'Command', 'variable': 'Variable'}[m]
    return hd
    
def amethyst(input):
    """Runs amethyst with the given input and returns the output."""
    p = subprocess.Popen(['amethyst', '-dWIKI'], 
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    (output, errs) = p.communicate(input)
    return output    

def inOtherCollection(title, excludeCol):
    """Check if a @a title exists in any other collection other
    than @a excludeCol."""
    for col in collections:
        if col == excludeCol: continue
        if title in pagesForCollection[col]:
            return True
    return False
    
    
class Page:
    """Wiki article about a console variable or command."""
    
    def __init__(self, title, content):
        self.title = title
        self.content = content
        self.name = ''
        self.summary = ''
        self.mode = ''
        
    def appendCollectionToTitle(self, col):
        if ')' in self.title:
            self.title = self.title.replace(')', ' in %s)' % col)
        else:
            self.title += ' (%s)' % col
        

pagesForCollection = {}

for col in collections:
    print 'Processing', col, 'documentation...'
    pagesForCollection[col] = {}
            
    for mode in modes:
        print '-', mode

        dirName = os.path.join(col, mode)
        files = os.listdir(dirName)
        for fn in files:
            if not fn.endswith('.ame'): continue
            name = fn[:-4]
            title = name

            if mode == 'command': title += " (Cmd)" 

            # Generate full page.            
            templ = '@require{amestd}\n'
            templ += '@macro{summary}{== Summary == @break @arg}\n'
            templ += '@macro{description}{@break == Description == @break @arg}\n'
            templ += '@macro{cbr}{<br/>}\n'
            templ += '@begin\n' + \
                 file(os.path.join(dirName, fn)).read()
                    
            templ += '\n[[Category:%s]]\n' % categoryForMode(mode)
            templ += '\n[[Category:%s]]\n' % categoryForCollection(col, mode)
                      
            page = Page(title, amethyst(templ))
            page.mode = mode
            pagesForCollection[col][title] = page
            
            # Just the summary for the index.
            templ = '@require{amestd}\n'
            templ += '@macro{summary}{@arg}\n'
            templ += '@macro{description}{}\n'
            templ += '@macro{cbr}{}\n'
            templ += '@begin\n' + \
                 file(os.path.join(dirName, fn)).read()
                 
            page.name = name
            page.summary = amethyst(templ).strip()
                                        
# Check for ambiguous pages.
ambigs = []
for col in collections:
    pages = pagesForCollection[col]
    for page in pages.values():
        if inOtherCollection(page.title, col):
            title = page.title
            ambigPage = Page(title, 
                "'''%s''' can be one of the following:\n" % title)
            # Must be more specific.
            for c in collections:
                if title in pagesForCollection[c]:
                    p = pagesForCollection[c][title]
                    p.appendCollectionToTitle(c)
                    ambigPage.content += '* [[%s]]\n' % p.title
            ambigs.append(ambigPage)
            
ambigs = sorted(ambigs, key=lambda p: p.title)
        
# Generate the indices.
indexPage = {}
indexPage['variable'] = ''
indexPage['command'] = ''

for col in collections:
    for mode in modes:
        indexPage[mode] += '\n== %s ==\n\n' % heading(col, mode)
        indexPage[mode] += '{| class="wikitable" width="100%%"\n! width="%i%%"| %s\n! Description\n' \
            % (colWidth(mode), colHeading(mode))
        
        pages = sorted(pagesForCollection[col].values(), key=lambda p: p.name)
        for page in filter(lambda p: p.mode == mode, pages):
            indexPage[mode] += '|-\n| [[%s|%s]]\n| %s\n' % (page.title, page.name, page.summary)

        indexPage[mode] += '|}\n'
        
for m in modes:
    indexPage[m] += '\n[[Category:Console]]\n[[Category:References]]\n'

        
dew.login()
        
dew.submitPage('Console command reference', indexPage['command'], comment)
dew.submitPage('Console variable reference', indexPage['variable'], comment)

print 'Submitting deambiguation pages...'
for amb in ambigs:
    print '-', amb.title
    dew.submitPage(amb.title, amb.content, comment)

print 'Submitting pages...'

for col in collections:
    pages = pagesForCollection[col]
    count = len(pages)
    i = 0
    for page in pages.values():
        i += 1
        print '-', col, ": %i / %i" % (i, count)
        dew.submitPage(page.title, page.content, comment)

dew.logout()