#!/usr/bin/env python2.7
# clang/gcc build log cleaner

import sys, re, string

def prefix_white(ln):
    pfx = ''
    for c in ln:
        if c in string.whitespace:
            pfx += c * 2
        else:
            break
    return pfx

lines   = file(sys.argv[1]).readlines()
erlines = file(sys.argv[2]).readlines()

print "Original log output contains %i+%i lines.\n" % (len(lines), len(erlines))

components = {}
qmakeOut = []

class Module:
    def __init__(self):
        self.name = ''
        self.location = ''
        self.cached = False

c = 0
for ln in lines:
    ln = ln.rstrip()

    c += 1

    # What kind of a line do we have?
    found = re.search('[a-z]+\.pro', ln)
    if found:
        print prefix_white(ln) + 'qmake ' + found.group(0)   
        continue

    
    #found = re.search('Entering directory.*/(deng/.*)\'', ln)
    #if found:
    #    print '\nBuilding in ' + found.group(1) + '...\n'
    #    continue
        
    found = re.search('.*(clang|gcc).*/doomsday/(.*/)([A-Za-z_0-9]+\.c[p]*)', ln)
    if found:
        mod = Module()
        mod.name = found.group(3)
        mod.location = found.group(2)
        mod.cached = ln.startswith('ccache')
        #print mod.name, mod.location, mod.cached
        if mod.location not in components:
            components[mod.location] = {}            
        components[mod.location][mod.name] = mod
        continue
        
class Issue:
    def __init__(self):
        self.name = ''
        self.location = ''
        self.kind = 'other'
        self.msg = []
        self.lineNumber = 0
        
# Look for errors.
issues = {}
current = None
for ln in erlines:
    ln = ln.rstrip()
    
    if current:
        if re.match('^[~^ ]+$', ln): 
            current = None
        else:
            current.msg.append(ln.strip())
        continue
    
    found = re.search('Project (MESSAGE|ERROR): (.*)', ln)
    if found:
        qmakeOut.append(found.group(2))
        continue    

    found = re.search('^([a-z0-9_/\.]+/[a-z_0-9]+\.[ch][p]*):[0-9]+', ln, re.IGNORECASE)
    if found:
        grp = re.search('/doomsday/(.*/)([a-z_0-9]+\.[ch][p]*)', found.group(1), re.IGNORECASE)
        if grp:
            kind = 'other'
            if ' warning:' in ln:
                kind = 'warning'
            elif ' error:' in ln:
                kind = 'error'
                
            msg = ''
            num = 0
            found = re.search('[chp]:([0-9]+):.*(warning|error): (.*)', ln)
            if found:
                num = int(found.group(1))
                msg = found.group(3)
                
            current = Issue()
            current.name = grp.group(2)
            current.location = grp.group(1)
            current.kind = kind
            current.lineNumber = num
            current.msg.append(msg.strip())
            
            if current.name not in issues: issues[current.name] = []
            issues[current.name].append(current)
            
            #print kind, grp.group(1), grp.group(2), msg
                
        

##############################################################################
# Produce output
##############################################################################
print
print string.join(qmakeOut, '\n')

# Group by component.
for compName in sorted(components):
    print '\n%-35s [%3i files]' % (compName, len(components[compName]))
    compIssues = []
    for modName in sorted(components[compName]):
        mod = components[compName][modName]
        # Any issues for this?
        status = ''
        warns = 0
        errs = 0
        if mod.name in issues:
            for issue in issues[mod.name]:
                if issue.location != compName: continue
                compIssues.append(issue)
                if issue.kind == 'error': errs += 1
                elif issue.kind == 'warning': warns += 1
        if errs > 0:
            status += '%i errors' % errs
        if warns > 0:
            if errs > 0: status += ' and '
            status += '%i warnings' % warns
        if len(status) == 0: #continue # Nothing to see here.
            status = 'ok'
        # Flags.
        flags = ''
        if mod.cached: flags = 'c'
        if len(flags) > 0: flags = '[%s]' % flags
        # Print it out.
        print ' %3s %-31s : %s' % (flags, mod.name, status)
        
    # Any issues for this component?
    if len(compIssues) > 0:
        for issue in compIssues:
            print '\n     %7s @ %s:%i: %s' % (issue.kind.upper(), issue.name, issue.lineNumber, string.join(issue.msg, '\n         '))

#for name in issues:
#    uniq = []
#    for issue in issues[name]:
#        combined = (name, issue.kind, issue.lineNumber, issue.msg)
#        if combined not in uniq: uniq.append(combined)
#    for name, kind, num, msg in uniq:
#        print name, kind, num, msg