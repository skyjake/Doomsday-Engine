# coding=utf-8
import os
import utils
from event import Event
import build_version
import config

class Entry:
    def __init__(self):
        self.subject = ''
        self.extra = ''
        self.author = ''
        self.date = ''
        self.link = ''
        self.hash = ''
        self.message = ''
        
    def setSubject(self, subject):
        self.extra = ''
        # Check that the subject lines are not too long.
        MAX_SUBJECT = 100
        if len(utils.collated(subject)) > MAX_SUBJECT:
            self.extra = '...' + subject[MAX_SUBJECT:] + ' '
            subject = subject[:MAX_SUBJECT] + '...'
        else:
            # If there is a single dot at the end of the subject, remove it.
            if subject[-1] == '.' and subject[-2] != '.':
                subject = subject[:-1]
        self.subject = subject
        
    def setMessage(self, message):
        self.message = message.strip()
        if self.extra:
            self.message = extra + ' ' + self.message
        self.message = self.message.replace('\n\n', '<br/><br/>').replace('\n', ' ').strip()
        

class Changes:
    def __init__(self, fromTag, toTag):
        self.fromTag = fromTag
        self.toTag = toTag
        self.parse()
        
    def parse(self):
        tmpName = '__ctmp'

        format = '[[Subject]]%s[[/Subject]]' + \
                 '[[Author]]%an[[/Author]]' + \
                 '[[Date]]%ai[[/Date]]' + \
                 '[[Link]]http://deng.git.sourceforge.net/git/gitweb.cgi?' + \
                 'p=deng/deng;a=commit;h=%H[[/Link]]' + \
                 '[[Hash]]%H[[/Hash]]' + \
                 '[[Message]]%b[[/Message]]'
        os.system("git log %s..%s --format=\"%s\" >> %s" % (self.fromTag, self.toTag, format, tmpName))

        logText = unicode(file(tmpName, 'rt').read(), 'utf-8')
        logText = logText.replace(u'ä', u'&auml;')
        logText = logText.replace(u'ö', u'&ouml;')
        logText = logText.replace(u'Ä', u'&Auml;')
        logText = logText.replace(u'Ö', u'&Ouml;')
        logText = logText.encode('utf-8')
        logText = logText.replace('<', '&lt;')
        logText = logText.replace('>', '&gt;')
        
        os.remove(tmpName)        

        pos = 0
        self.entries = []
        self.debChangeEntries = []
        while True:
            entry = Entry()
            
            pos = logText.find('[[Subject]]', pos)
            if pos < 0: break # No more.
            end = logText.find('[[/Subject]]', pos)
            
            entry.setSubject(logText[pos+11:end])

            # Debian changelog just gets the subjects.
            if entry.subject not in self.debChangeEntries:
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
            entry.setMessage(logText[pos+11:end])            
            
            self.entries.append(entry)
        
        
    def generate(self, format):
        fromTag = self.fromTag
        toTag = self.toTag
        
        if format == 'html':
            out = file(Event(toTag).file_path('changes.html'), 'wt')
            print >> out, '<ol>'

            # Write a list entry for each commit.
            for entry in self.entries:
                print >> out, '<li><b>%s</b><br/>' % entry.subject
                print >> out, 'by <i>%s</i> on %s' % (entry.author, entry.date)
                print >> out, '<a href="%s">(show in repository)</a>' % entry.link
                print >> out, '<blockquote>%s</blockquote>' % entry.message
                    
            print >> out, '</ol>'
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
                print >> out, '<title>%s</title>' % entry.subject
                if len(entry.message):
                    print >> out, '<message>%s</message>' % entry.message
                print >> out, '</commit>'                
            print >> out, '</commits>'
            out.close()
            
        elif format == 'deb':
            # Append the changes to the debian package changelog.
            os.chdir(os.path.join(config.DISTRIB_DIR, 'linux'))

            # First we need to update the version.
            build_version.find_version()
            debVersion = build_version.DOOMSDAY_VERSION_FULL_PLAIN + '-' + Event().tag()

            # Always make one entry.
            print 'Marking new version...'
            msg = 'New release: %s build %i.' % (build_version.DOOMSDAY_RELEASE_TYPE,
                                                 Event().number())
            os.system("dch --check-dirname-level 0 -v %s \"%s\"" % (debVersion, msg))       

            for entry in self.debChangeEntries:
                # Quote it for the command line.
                qch = entry.replace('"', '\\"').replace('!', '\\!')
                print ' *', qch
                os.system("dch --check-dirname-level 0 -a \"%s\"" % qch)
