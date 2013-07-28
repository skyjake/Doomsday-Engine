#!/usr/bin/python
# coding=utf-8
#
# Repository Codex Generator by <jaakko.keranen@iki.fi>
# Generates a set of web pages out of tags in Git commit headlines.
# License: GNU GPL version 2 (or later)

import os, sys, time, string, base64

OUT_DIR = 'codex'
TITLE = 'Doomsday Codex'

if sys.argv > 1:
    OUT_DIR = sys.argv[1]

class Commit:
    def __init__(self, subject, author, date, link, hash):
        self.subject = subject
        self.author = author
        self.date = date
        self.link = link
        self.hash = hash
        self.extract_tags()
        
    def add_tag(self, tag):
        if len(tag) <= 1: return
        if tag.lower() not in map(lambda x: x.lower(), self.tags):
            self.tags.append(tag)
        
    def extract_tags(self):
        self.tags = []
        
        sub = self.subject
        
        if sub.startswith('Revert "') and sub[-1] == '"':
            sub = sub[8:-1]
            self.tags.append(u'Revert')
                        
        self.subject = sub[sub.find(': ') + 1:].strip()
                    
        # Some tags are automatically applied based on the subject line.
        sensitiveWords = self.subject.replace(';', ' ').replace(':', ' ').replace(',', ' ').split(' ')
        words = map(lambda w: w.lower(), sensitiveWords)
        if 'fixed' in words: self.add_tag(u'Fixed')
        if 'cleanup' in words: self.add_tag(u'Cleanup')
        if 'debug' in words: self.add_tag(u'Debug')
        if 'fixed' in words: self.add_tag(u'Fixed')
        if 'added' in words: self.add_tag(u'Added')
        if 'refactor' in self.subject.lower(): self.add_tag(u'Refactor')
        if 'GL' in sensitiveWords: self.add_tag(u'GL')
        if 'performance' in words: self.add_tag(u'Performance')
        if 'optimize' in words or 'optimized' in words or 'optimizing' in words: 
            self.add_tag(u'Optimize')

        # Are there any actual tags specified?
        if sub.find(': ') < 0: return
                                    
        sub = sub[:sub.find(': ')].replace(',', '|').replace('/', '|').replace('(', '|').\
            replace('&', '|').replace(')', '').replace('Changed ', 'Changed|')
                
        for unclean in sub.split('|'):
            unclean = unclean.replace('*nix', 'Unix')
            tag = unclean.replace('[', '').replace(']', '').replace('*', '').replace('"', '').strip()
            if 'ccmd' in tag.lower() or 'cvar' in tag.lower(): tag = u'Console'
            if 'cleanup' in tag.lower() or 'clean up' in tag.lower(): tag = u'Cleanup'
            if '_' in tag: continue
            if tag.lower() == 'chex':
                tag = u'Chex Quest'
            if tag.lower() == 'hacx':
                tag = u'HacX'
            if 'common' in tag.lower():
                tag = u'libcommon'
            if 'doom64' in tag.lower():
                tag = u'Doom64'
            elif 'doom' in tag.lower():
                tag = u'Doom'
            elif 'heretic' in tag.lower():
                tag = u'Heretic'
            elif 'hexen' in tag.lower():
                tag = u'Hexen'            
            if tag.lower() == 'add-on repository' or tag.lower() == 'addon repository':
                tag = u'Add-on Repository'
            if tag.lower() == 'bsp builder' or tag.lower() == 'bspbuilder':
                tag = u'BSP Builder'
            if tag.lower() == 'build repository' or tag.lower() == 'buildrepository':
                tag = u'Build Repository'
            if tag.lower() == 'deh reader' or tag.lower() == 'dehreader' or \
                    tag.lower() == 'deh read' or tag.lower() == 'dehacked reader' or \
                    tag.lower() == 'dehread':
                tag = u'Deh Reader'
            if tag.lower() == 'dpdehread':
                tag = u'dpDehRead'
            if tag.lower() == 'gcc':
                tag = u'GCC'
            if tag.lower() == 'glsandbox':
                tag = u'GLSandbox'
            if tag.lower() == 'dsdirectsound':
                tag = u'dsDirectSound'
            if tag.lower() == 'busy mode':
                tag = u'Busy Mode'
            if tag.lower() == 'dedicated server':
                tag = u'Dedicated Server'
            if tag.lower() == 'cmake':
                tag = u'CMake'
            if tag.lower().startswith('filesys') or tag.lower().startswith('file sys'):
                tag = u'File System'
            if tag.lower() == 'clang':
                tag = u'Clang'
            if 'zone' in tag.lower():
                tag = u'Memory Zone'
            if tag.lower() == 'infine':
                tag = u'InFine'
            if 'all games' in tag.lower():
                self.add_tag(u'All Games')
            if tag.lower() == 'lightgrid':
                tag = u'Light Grid'
            if tag.lower() == 'render':
                tag = u'Renderer'
            if tag.lower() == 'sfx':
                tag = u'SFX'
            if tag.lower() == 'taskbarwidth' or tag.lower() == 'taskbar' or tag.lower() == 'taskbarwidget':
                tag = u'Task Bar'
            if tag.lower() == 'texture manager':
                tag = u'Texture Manager'
            if 'refactoring' in tag.lower() or 'refactored' in tag.lower():
                tag = u'Refactor'
            if 'optimization' in tag.lower() or 'optimize' in tag.lower():
                tag = u'Optimize'
            if tag == 'osx' or tag.lower() == 'mac' or tag.lower() == 'mac os x':
                tag = u'OS X'
            if tag.lower().startswith('fixed'):
                tag = tag[5:].strip()
                self.add_tag(u'Fixed')
            if tag.lower().startswith('added'):
                tag = tag[5:].strip()
                self.add_tag(u'Added')
            if len(tag) > 2 and (tag[0] == '(' and tag[-1] == ')'):
                tag = tag[1:-1]
            if tag == u'WadMapConverter':
                tag = u'Wad Map Converter'
            if tag.lower() == 'win32':
                tag = u'Windows'
            self.add_tag(tag)        
        
def contains_tag(subject):
    pos = subject.find(': ')
    if pos < 0:
        # It might still have some recognized words.
        for allowed in ['debug', 'optimize', 'cleanup', 'fixed', 'added', 'refactor']:
            if allowed in subject.lower():
                return True
    if pos > 60: return False
    for badWord in ['related ', ' up ', ' the ', ' a ', ' in ', ' of', ' on ', 
                    ' and ', ' then ', ' when', ' to ',  ' for ', ' with ', ' into ']:
        if badWord in subject[:pos]:
            return False
    if subject[pos-4:pos+1] == 'TODO:': return False
    # It can only contain colons after the tag marker.
    p = subject.find(':')
    if p >= 0 and p < pos: return False
    p = subject.find('.')
    if p >= 0 and p < pos: return False
    return True

def fetch_commits():
    commits = {}

    tmpName = '__ctmp'
    format = '[[Subject]]%s[[/Subject]]' + \
             '[[Author]]%an[[/Author]]' + \
             '[[Date]]%ai[[/Date]]' + \
             '[[Link]]http://github.com/skyjake/Doomsday-Engine/commit/%H[[/Link]]' + \
             '[[Hash]]%H[[/Hash]]'
    os.system("git log --format=\"%s\" >> %s" % (format, tmpName))
    logText = unicode(file(tmpName, 'rt').read(), 'utf-8')
    os.remove(tmpName)        

    pos = 0
    while True:    
        pos = logText.find('[[Subject]]', pos)
        if pos < 0: break # No more.
        end = logText.find('[[/Subject]]', pos)        
        subject = logText[pos+11:end]

        # Author.
        pos = logText.find('[[Author]]', pos)
        end = logText.find('[[/Author]]', pos)
        author = logText[pos+10:end]
        
        # Date.
        pos = logText.find('[[Date]]', pos)
        end = logText.find('[[/Date]]', pos)
        date = logText[pos+8:end]
        
        # Link.
        pos = logText.find('[[Link]]', pos)
        end = logText.find('[[/Link]]', pos)
        link = logText[pos+8:end]

        # Hash.
        pos = logText.find('[[Hash]]', pos)
        end = logText.find('[[/Hash]]', pos)
        hash = logText[pos+8:end]
        
        if contains_tag(subject):        
            commits[hash] = Commit(subject, author, date, link, hash)

    return commits

# Compile the commit database.
byHash = fetch_commits()
byTag = {}
for commit in byHash.values():
    # Index all commits by tag.
    for tag in commit.tags:
        if tag in byTag:
            if commit not in byTag[tag]:
                byTag[tag].append(commit)
        else:
            byTag[tag] = [commit]
          
relatedTags = [
    ['Fixed', 'Added', 'Refactor', 'Optimize', 'Revert', 'Cleanup', 'Debug'],
    ['Windows', 'OS X', 'Linux', 'Unix', 'Debian', 'Ubuntu', 'FreeBSD', 'X11', '64-bit'],
    ['Windows', 'Windows *'],
    ['Qt', 'SDL'],
    ['Builder', 'Builds', 'qmake', 'Project*', 'CMake', 'Distrib', 'GCC', 'MSVC', 'Clang', 'Git', 
     'TextMate', 'wikidocs'],
    ['Test*', 'GLSandbox'],
    ['Docs', 'Documentation', 'Codex', 'Readme', 'Doxygen', 'Amethyst'],
    ['Homepage', 'Add-on Repository', 'Build Repository', 'CSS', 'RSS', 'DEW', 'Forums'],
    ['libdeng2', 'App', 'Config', 'Log', 'LogBuffer', 'Widgets'],
    ['libgui', 'DisplayMode', 'GL', 'OpenGL', 'GL*', 'Atlas*', 'Image', '*Bank', 'Font'],
    ['libdeng', 'libdeng1', 'Garbage', 'Garbages', 'Str'],
    ['libshell', 'Shell', 'AbstractLineEditor', 'Link'],    
    ['Client', 'Client UI', 'UI', 'Console', 'Control Panel', 'Default Style', 'GameSelectionWidget',
     'Task Bar', 'Updater', 'WindowSystem', 'Client*'],
    ['Server', 'Dedicated Server', 'Dedicated Mode', 'Server*'],
    ['Snowberry', 'Shell', 'Amethyst', 'md2tool'],
    ['All Games', 'Doom', 'Heretic', 'Hexen', 'Doom64', 'Chex Quest', 'HacX'],
    ['Plugins', 'Wad Map Converter', 'Deh Reader', 'dsDirectSound', 'OpenAL', 'dpDehRead',
     'dsWinMM', 'Dummy Audio', 'Example Plugin', 'exampleplugin', 'FluidSynth', 'FMOD'],
    ['API', 'DMU', 'DMU API'],
    ['DED', 'DED Parser', 'Ded Reader', 'Definitions', 'Info', 'ScriptedInfo'],
    ['Ring Zero', 'GameSelectionWidget'],
    ['Script*', 'scriptsys', 'Script', 'Record', 'Variable'],
    ['File System', 'Folder', 'Feed', 'FS', 'FS1', 'File*', '*File'],
    ['Resource*', 'Material*', 'Texture*', 'Uri'],
    ['Renderer', '* Renderer', 'Model*', 'Light Grid'],
    ['Network', 'Multiplayer', 'Server', 'Protocol'],
    ['Concurrency', 'Task', 'libdeng2'],
    ['libcommon', 'Game logic', 'Menu', 'Game Menu', 'Game Save', 'Games', 'Automap'],
    ['World', 'GameMap', 'Map', 'BSP Builder', 'HEdge', 'Bsp*', 'Line*', 'Sector', 'SectionEdge', 'Wall*',
     'Blockmap', 'Polyobj', 'Plane'],
    ['Finale Interpreter', 'Finales', 'InFine'],
    ['Input*', 'Bindings', 'Joystick', 'MouseEvent', 'KeyEvent', 'libgui'],
    ['Audio*', 'SFX', 'Music', 'FluidSynth'],
    ['Widget*', '*Widget', '*Rule', 'Rule*', 'Animation' ]
]

def find_related_tags(tag):
    rels = []
    for group in relatedTags:
        # Any wildcards?
        tags = []
        for t in group:
            if '*' not in t:
                tags.append(t)
            elif t[0] == '*':
                for x in byTag.keys():
                    if x.endswith(t[1:]):
                        tags.append(x)
            elif t[-1] == '*':
                for x in byTag.keys():
                    if x.startswith(t[:-1]):
                        tags.append(x)                
        if tag not in tags: continue
        for t in tags:
            if t != tag and t not in rels:
                rels.append(t)            
    return sorted(rels, key=lambda s: s.lower())
          
if not os.path.exists(OUT_DIR): os.mkdir(OUT_DIR)          

def tag_filename(tag):
    try:
        return base64.urlsafe_b64encode(tag)
    except:
        print 'FAILED:', tag.encode('utf8')
        sys.exit(1)
                                            
def print_header(out, pageTitle):
    print >> out, '<!DOCTYPE html>'
    print >> out, '<html>'
    print >> out, '<head><title>%s - %s</title>' % (pageTitle, TITLE)
    print >> out, '<style type="text/css">'
    print >> out, 'body, table { font-family: "Helvetica Neue", sans-serif; }'
    print >> out, 'a { color: black; text-decoration: none; }'
    print >> out, 'a:hover { text-decoration: underline; }'
    print >> out, '.skiplist { line-height: 150%; }'
    print >> out, '</style>'
    print >> out, '</head>'
    print >> out, '<body>'
    print >> out, '<form id="findform" name="find" action="find_tag.php" method="get"><a href="index.html">Alphabetical Index</a> | <a href="index_by_size.html">Tags by Size</a> | Find tag: <input type="text" name="tag"> <input type="submit" value="Find"></form>'
    print >> out, '<h1>%s</h1>' % pageTitle
    
def print_footer(out):
    print >> out, '<center><p>&oplus;</p><small><p>This is the <a href="http://dengine.net/">Doomsday Engine</a> <a href="http://github.com/skyjake/Doomsday-Engine/">source code repository</a> commit tag index.</p><p>Last updated: %s</small></center>' % time.asctime()
    print >> out, '</body>'
    print >> out, '</html>'

def print_table(out, cells, cell_printer, numCols=4, tdStyle='', separateByFirstLetter=False, letterFunc=None):
    colSize = (len(cells) + numCols - 1) / numCols
    tdElem = '<td valign="top" width="%i%%" style="%s">' % (100/numCols, tdStyle)
    print >> out, '<table width="100%"><tr>' + tdElem

    idx = 0
    inCol = 0
    letter = ''
    
    while idx < len(cells):
        if separateByFirstLetter:
            if inCol > 0 and letter != '' and letter != letterFunc(cells[idx]):
                print >> out, '<br/>'
            letter = letterFunc(cells[idx])
            
        cell_printer(out, cells[idx])
    
        idx += 1
        inCol += 1
        if inCol == colSize:
            print >> out, tdElem
            inCol = 0
    
    print >> out, '</table>'
            
# Create the index page with all tags sorted alphabetically.
out = file(os.path.join(OUT_DIR, 'index.html'), 'wt')
print_header(out, 'Alphabetical Tag Index')

sortedTags = sorted(byTag.keys(), cmp=lambda a, b: cmp(a.lower(), b.lower()))

def alpha_index_cell(out, tag):
    count = len(byTag[tag])
    if count > 100:
        style = 'font-weight: bold'
    elif count < 5:
        style = 'color: #aaa'
    else:
        style = ''
    print >> out, '<a href="tag_%s.html"><span style="%s">%s (%i)</span></a></br>' % (
        tag_filename(tag), style, tag, count)

print_table(out, sortedTags, alpha_index_cell, separateByFirstLetter=True,
            letterFunc=lambda t: t[0].lower())

print_footer(out)

# Create the index page with tags sorted by size.
out = file(os.path.join(OUT_DIR, 'index_by_size.html'), 'wt')
print_header(out, 'All Tags by Size')

def tag_size_comp(a, b):
    c = cmp(len(byTag[b]), len(byTag[a]))
    if c == 0: return cmp(a.lower(), b.lower())
    return c

def size_index_cell(out, tag):
    count = len(byTag[tag])
    if count > 100:
        style = 'font-weight: bold'
    elif count < 5:
        style = 'color: #aaa'
    else:
        style = ''
    print >> out, '<a href="tag_group_%s.html"><span style="%s">%i %s</span></a></br>' % (
        tag_filename(tag), style, count, tag)    

print_table(out, sorted(byTag.keys(), cmp=tag_size_comp), size_index_cell)

print_footer(out)

def encoded_text(logText):
    logText = logText.replace(u'&', u'&amp;')
    logText = logText.replace(u'ä', u'&auml;')
    logText = logText.replace(u'ö', u'&ouml;')
    logText = logText.replace(u'Ä', u'&Auml;')
    logText = logText.replace(u'Ö', u'&Ouml;')
    logText = logText.replace(u'<', u'&lt;')
    logText = logText.replace(u'>', u'&gt;')
    logText = filter(lambda c: c in string.whitespace or c > ' ', logText)
    return logText
    
def print_tags(out, com, tag, linkSuffix=''):
    first = True
    for other in sorted(com.tags, cmp=lambda a, b: cmp(a.lower(), b.lower())):
        if other != tag:
            if not first: print >> out, '| '
            first = False
            print >> out, '<b><a href="tag_%s%s.html">%s</a></b>' % (linkSuffix, tag_filename(other), other)
    
def print_commit(out, com, tag, linkSuffix=''):
    print >> out, trElem + '<td valign="top" width="100" style="padding-left:1.5ex;"><a href="%s">' % com.link + com.date[:10] + '</a> '
    print >> out, '<td valign="top" width="200" align="right">'
    print_tags(out, com, tag, linkSuffix)
    print >> out, '<td valign="top">:'
    print >> out, ('<td valign="top"><a href="%s">' % com.link + encoded_text(com.subject)[:250] + '</a>').encode('utf8')

colors = ['#f00', '#0a0', '#00f', 
          '#ed0', '#f80', '#0ce', 
          '#88f', '#b0b',  
          '#f99', '#8d8', '#222',
          '#44a', '#940', '#888']

def html_color(colIdx):
    if colIdx == None: return '#000'
    return colors[colIdx % len(colors)]

def color_box(colIdx):
    return ' <span style="padding-left:1em; background-color:%s">&nbsp;</span> ' % \
        html_color(colIdx)

def print_date_sorted_commits(out, coms, tag, linkSuffix='', colorIdx=None):
    months = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August',
              'September', 'October', 'November', 'December']
    curDate = ''
    dateSorted = sorted(coms, key=lambda c: c.date, reverse=True)
    print >> out, '<table width="100%">'
    for com in dateSorted:
        # Monthly headers.
        if curDate == '' or curDate != com.date[:7]:
            print >> out, '<tr><td colspan="4" style="padding-left:1ex; padding-top:1.5em; padding-bottom:0.5em; font-weight:bold; font-size:110%%; color:%s; border-bottom:1px dotted %s">' % \
                (html_color(colorIdx), html_color(colorIdx))
            print >> out, months[int(com.date[5:7]) - 1], com.date[:4]
        curDate = com.date[:7]        
        print_commit(out, com, tag, linkSuffix)
    print >> out, '</table>'

# Create the tag redirecting PHP page.
out = file(os.path.join(OUT_DIR, 'find_tag.php'), 'wt')
print >> out, '<?php'

print >> out, '$tags = array(', string.join(['"%s" => "%s"' % (tag, tag_filename(tag)) for tag in byTag.keys()], ', '), ');'
print >> out, """
$input = stripslashes(strip_tags($_GET["tag"]));
$style = $_GET["style"];
if($style == 'grouped') {
  $style = 'group_';
} else {
  $style = '';
}
$destination = "index.html";
$best = -1.0;
if(strlen($input) > 0 && strlen($input) < 60) {
  foreach($tags as $tag => $link) {
    $lev = (float) levenshtein($input, $tag); // case sensitive
    if(stripos($tag, $input) !== FALSE || stripos($input, $tag) !== FALSE) {
        // Found as a case insensitive substring, increase likelihood.
        $lev = $lev/2.0;
    }
    if(stripos($tag, $input) === 0) {
        // Increase likelihood further if the match is in the beginning.
        $lev = $lev/2.0;
    }
    if(!strcasecmp($tag, $input) == 0) {
        // Case insensitive direct match, increase likelihood.
        $lev = $lev/2.0;
    }
    if($lev == 0) {
      $destination = "tag_$style$link.html";
      break;
    }
    if($best < 0 || $lev < $best) {
      $destination = "tag_$style$link.html";
      $best = $lev;
    }
  }
}
header("Location: $destination");
"""

print >> out, "?>"

def print_related_tags(out, tag, style=''):
    rels = find_related_tags(tag)
    if len(rels) == 0: return
    print >> out, '<p>Related tags: '
    print >> out, string.join(['<a href="tag_%s%s.html">%s</a>' % (style, tag_filename(t), t)
                               for t in rels], ', ')
    print >> out, '</p>'

def print_authorship(out, tag):
    authors = {}
    total = len(byTag[tag])
    for commit in byTag[tag]:    
        if commit.author in authors:
            authors[commit.author] += 1
        else:
            authors[commit.author] = 1
    sortedAuthors = sorted(authors.keys(), key=lambda a: authors[a], reverse=True)    
    print >> out, '<p>Authorship:', string.join(['%i%% %s' % (100 * authors[a] / total, a) 
                                                 for a in sortedAuthors], ', ').encode('utf8'), '</p>'
    
#
# Create pages for each tag.
#
for tag in byTag.keys():
    #print 'Generating tag page:', tag

    trElem = '<tr style="padding:1ex">'
    
    # First a simple date-based list of commits in this tag.
    out = file(os.path.join(OUT_DIR, 'tag_%s.html' % tag_filename(tag)), 'wt')
    print_header(out, tag)
    print_related_tags(out, tag)
    print_authorship(out, tag)
    print >> out, '<p><a href="tag_group_%s.html"><b>View commits by groups</b></a></p>' % tag_filename(tag)
    print >> out, '<div style="margin-left:1em">'
    print_date_sorted_commits(out, byTag[tag], tag)
    print >> out, '</div>'
    print_footer(out)
    
    present = {}
    for com in byTag[tag]:
        for other in com.tags:
            if other == tag: continue
            if other not in present:
                present[other] = [com]
            else:
                present[other].append(com)

    def present_by_size(a, b):
        c = cmp(len(present[b]), len(present[a]))
        if c == 0: return cmp(a.lower(), b.lower())
        return c
        
    presentSorted = sorted(present.keys(), cmp=present_by_size)
    
    # Then grouped by subgroup size.            
    out = file(os.path.join(OUT_DIR, 'tag_group_%s.html' % tag_filename(tag)), 'wt')
    print_header(out, tag + ' (Grouped)')
    print_related_tags(out, tag, 'group_')
    print_authorship(out, tag)
    print >> out, '<p><a id="top"></a><a href="tag_%s.html"><b>View commits by date</b></a></p>' % tag_filename(tag)

    if len(byTag[tag]) > 10:
        print >> out, '<p class="skiplist"><b>Jump down to:</b> '

        class SkipTagPrinter:
            def __init__(self):
                self.color = 0
            
            def __call__(self, out, tag):
                print >> out, '<span style="white-space:nowrap;">' + color_box(self.color) 
                self.color += 1
                print >> out, '<a href="#%s">%s (%i)</a></span></br>' % (tag_filename(tag),
                    tag, len(present[tag]))
    
        print_table(out, presentSorted, SkipTagPrinter(), numCols=5, tdStyle='line-height:150%')
        
        print >> out, '</p>'
    
    # First the commits without any subgroups.
    print >> out, '<div style="margin-left:1em">'
    print_date_sorted_commits(out, filter(lambda c: len(c.tags) == 1, byTag[tag]), tag)    
    print >> out, '</div>'

    color = 0
    for tag2 in presentSorted:
        print >> out, '<a id="%s"><h2>%s<a href="tag_group_%s.html">%s</a> (%i) <span style="color:#ccc">&mdash; %s</span></h2></a>' % (
            tag_filename(tag2), color_box(color), tag_filename(tag2), tag2, len(present[tag2]), tag)
        print >> out, '<div style="margin-left:0.85em; border-left:4px solid %s">' % html_color(color)
        print_date_sorted_commits(out, filter(lambda c: tag2 in c.tags, byTag[tag]), tag, 'group_',
            colorIdx=color)
        print >> out, '</div>'
        print >> out, '<p><small><a href="#top">&uarr; Back to top</a></small></p>'
        color += 1
    
    print_footer(out)
