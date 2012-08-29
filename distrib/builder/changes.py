# coding=utf-8
import os
import string
import utils
from event import Event
import config

def encodedText(logText):
    logText = logText.replace('&', '&amp;')
    logText = logText.replace(u'ä', u'&auml;')
    logText = logText.replace(u'ö', u'&ouml;')
    logText = logText.replace(u'Ä', u'&Auml;')
    logText = logText.replace(u'Ö', u'&Ouml;')
    logText = logText.encode('utf-8')
    logText = logText.replace('<', '&lt;')
    logText = logText.replace('>', '&gt;')
    logText = filter(lambda c: c in string.whitespace or c > ' ', logText)
    return logText


class Entry:
    def __init__(self):
        self.subject = ''
        self.extra = ''
        self.author = ''
        self.date = ''
        self.link = ''
        self.hash = ''
        self._message = ''
        self.tags = []
        self.guessedTags = []
        self.reverted = False
        
    def set_subject(self, subject):
        self.extra = ''
        
        # Check for a revert.
        if subject.startswith('Revert "') and subject[-1] == '"':
            self.reverted = True
            subject = subject[8:-1]
        
        # Remote tags from the subject.
        pos = subject.find(': ')
        if pos > 0:
            for tag in subject[:pos].split('|'):
                self.tags.append(tag.strip())
            subject = subject[pos + 1:].strip()
        
        if len(subject) == 0: subject = "Commit"
        
        # Check that the subject lines are not too long.
        MAX_SUBJECT = 100
        if len(utils.collated(subject)) > MAX_SUBJECT:
            # Find a suitable spot the break the subject line.
            pos = MAX_SUBJECT
            while subject[pos] not in string.whitespace: pos -= 1
            self.extra = '...' + subject[pos+1:] + ' '
            subject = subject[:pos] + '...'
        else:
            # If there is a single dot at the end of the subject, remove it.
            if subject[-1] == '.' and subject[-2] != '.':
                subject = subject[:-1]
        self.subject = encodedText(subject)
        
    def set_message(self, message):
        self._message = message.strip()
        if self.extra:
            self._message = self.extra + ' ' + self._message
        self._message = encodedText(self._message)
        
    def message(self, encodeHtml=False):
        if encodeHtml:
            return self._message.replace('\n\n', '<br/><br/>').replace('\n', ' ').strip()
        return self._message.strip()
        

class Changes:
    def __init__(self, fromTag, toTag):
        self.fromTag = fromTag
        self.toTag = toTag
        self.parse()
        
    def should_ignore(self, subject):
        if subject.startswith("Merge branch") or subject.startswith("Merge commit"):
            # Branch merges are not listed.
            return True
        return False
        
    def parse(self):
        tmpName = '__ctmp'

        format = '[[Subject]]%s[[/Subject]]' + \
                 '[[Author]]%an[[/Author]]' + \
                 '[[Date]]%ai[[/Date]]' + \
                 '[[Link]]http://sourceforge.net/p/deng/code/ci/%H/[[/Link]]' + \
                 '[[Hash]]%H[[/Hash]]' + \
                 '[[Message]]%b[[/Message]]'
        os.system("git log %s..%s --format=\"%s\" >> %s" % (self.fromTag, self.toTag, format, tmpName))

        logText = unicode(file(tmpName, 'rt').read(), 'utf-8')
        
        os.remove(tmpName)        

        pos = 0
        self.entries = []
        self.debChangeEntries = []
        while True:
            entry = Entry()
            
            pos = logText.find('[[Subject]]', pos)
            if pos < 0: break # No more.
            end = logText.find('[[/Subject]]', pos)
            
            entry.set_subject(logText[pos+11:end])

            # Debian changelog just gets the subjects.
            if entry.subject not in self.debChangeEntries and not \
                self.should_ignore(entry.subject):
                self.debChangeEntries.append(entry.subject)

            # Author.
            pos = logText.find('[[Author]]', pos)
            end = logText.find('[[/Author]]', pos)
            entry.author = logText[pos+10:end]
            
            # Date.
            pos = logText.find('[[Date]]', pos)
            end = logText.find('[[/Date]]', pos)
            entry.date = logText[pos+8:end]
            
            # Link.
            pos = logText.find('[[Link]]', pos)
            end = logText.find('[[/Link]]', pos)
            entry.link = logText[pos+8:end]

            # Hash.
            pos = logText.find('[[Hash]]', pos)
            end = logText.find('[[/Hash]]', pos)
            entry.hash = logText[pos+8:end]

            # Message.
            pos = logText.find('[[Message]]', pos)
            end = logText.find('[[/Message]]', pos)
            entry.set_message(logText[pos+11:end])            
            
            if not self.should_ignore(entry.subject):
                self.entries.append(entry)

        self.deduce_tags()
        self.remove_reverts()
                
    def all_tags(self):
        # These words are always considered to be valid tags.
        tags = ['Cleanup', 'Fixed', 'Added', 'Refactor', 'Performance', 'Optimize', 'Merge branch']
        for e in self.entries:
            for t in e.tags + e.guessedTags:
                if t not in tags: 
                    tags.append(t)
        return tags

    def is_reverted(self, entry):
        for e in self.entries:
            if e.reverted and e.subject == entry.subject and e.tags == entry.tags:
                return True
        return False

    def remove_reverts(self):
        self.entries = filter(lambda e: not self.is_reverted(e), self.entries)

    def deduce_tags(self):
        # Look for known tags in untagged titles.
        allTags = self.all_tags()
        for entry in self.entries:
            if entry.tags: continue
            # This entry has no tags yet.    
            for tag in allTags:
                p = entry.subject.lower().find(tag.lower())
                if p < 0: continue
                if p == 0 or entry.subject[p - 1] not in string.ascii_letters + '-_':
                    entry.guessedTags.append(tag)
        
    def form_groups(self, allEntries):
        groups = {}
        for tag in self.all_tags():
            groups[tag] = []
            for e in allEntries:
                if tag in e.tags:
                    groups[tag].append(e)
            for e in allEntries:
                if tag in e.guessedTags:
                    groups[tag].append(e)
            
            for e in groups[tag]:
                allEntries.remove(e)
        
        groups['Miscellaneous'] = []
        for e in allEntries:
            groups['Miscellaneous'].append(e)
            
        return groups
    
    def pretty_group_list(self, tags):
        listed = ''
        if len(tags) > 1:
            listed = string.join(tags[:-1], ', ')
            listed += ' and ' + tags[-1]
        elif len(tags) == 1:
            listed = tags[0]
        return listed
            
    def generate(self, format):
        fromTag = self.fromTag
        toTag = self.toTag
        
        if format == 'html':
            out = file(Event(toTag).file_path('changes.html'), 'wt')

            MAX_COMMITS = 100
            entries = self.entries[:MAX_COMMITS]
            
            if len(self.entries) > MAX_COMMITS:
                print >> out, '<p>Showing %i of %i commits.' % (MAX_COMMITS, len(self.entries))
                print >> out, 'The <a href="%s">oldest commit</a> is dated %s.</p>' % \
                    (self.entries[-1].link, self.entries[-1].date)                
            
            # Form groups.
            groups = self.form_groups(entries)
            keys = groups.keys()
            # Sort case-insensitively by group name.
            keys.sort(cmp=lambda a, b: cmp(str(a).lower(), str(b).lower()))
            for group in keys:
                if not len(groups[group]): continue
                
                print >> out, '<h3>%s</h3>' % group                                
                print >> out, '<ul>'

                # Write a list entry for each commit.
                for entry in groups[group]:
                    otherGroups = []
                    for tag in entry.tags + entry.guessedTags:
                        if tag != group:
                            otherGroups.append(tag)
                            
                    others = self.pretty_group_list(otherGroups)
                    if others: others = ' (&rarr; %s)' % others
                    
                    print >> out, '<li>'    
                    print >> out, '<a href="%s">%s</a>: ' % (entry.link, entry.date[:10])
                    print >> out, '<b>%s</b>' % entry.subject
                    print >> out, 'by <i>%s</i>%s' % (entry.author, others)
                    print >> out, '<blockquote style="color:#666;">%s</blockquote>' % entry.message(encodeHtml=True)
                    
                print >> out, '</ul>'
            out.close()
            
        elif format == 'xml':
            out = file(Event(toTag).file_path('changes.xml'), 'wt')
            print >> out, '<commitCount>%i</commitCount>' % len(self.entries)
            print >> out, '<commits>'
            for entry in self.entries:
                print >> out, '<commit>'
                print >> out, '<submitDate>%s</submitDate>' % entry.date
                print >> out, '<author>%s</author>' % entry.author
                print >> out, '<repositoryUrl>%s</repositoryUrl>' % entry.link
                print >> out, '<sha1>%s</sha1>' % entry.hash
                if entry.tags or entry.guessedTags:
                    print >> out, '<tags>'
                    for t in entry.tags:
                        print >> out, '<tag guessed="false">%s</tag>' % t
                    for t in entry.guessedTags:
                        print >> out, '<tag guessed="true">%s</tag>' % t
                    print >> out, '</tags>'
                print >> out, '<title>%s</title>' % entry.subject
                if len(entry.message()):
                    print >> out, '<message>%s</message>' % entry.message()
                print >> out, '</commit>'                
            print >> out, '</commits>'
            out.close()
            
        elif format == 'deb':
            import build_version
            build_version.find_version()

            # Append the changes to the debian package changelog.
            os.chdir(os.path.join(config.DISTRIB_DIR, 'linux'))

            # First we need to update the version.
            if build_version.DOOMSDAY_RELEASE_TYPE == 'Stable':
                debVersion = build_version.DOOMSDAY_VERSION_FULL
            else:
                debVersion = build_version.DOOMSDAY_VERSION_FULL + '-' + Event().tag()

            # Always make one entry.
            print 'Marking new version...'
            msg = 'New release: %s build %i.' % (build_version.DOOMSDAY_RELEASE_TYPE,
                                                 Event().number())
            os.system("dch -b --check-dirname-level 0 -v %s \"%s\"" % (debVersion, msg))       

            for entry in self.debChangeEntries:
                # Quote it for the command line.
                qch = entry.replace('"', '\\"').replace('!', '\\!')
                print ' *', qch
                os.system("dch --check-dirname-level 0 -a \"%s\"" % qch)
