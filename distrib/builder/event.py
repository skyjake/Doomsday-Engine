import os, glob, shutil, time
import build_number
import config
import utils

def log_filename(package, osIdent, ext='txt.gz'):
    return 'buildlog-%s-%s.%s' % (package, osIdent, ext)


class Event:
    """Build event. Manages the contents of a single build directory under
    the event directory."""
    
    def __init__(self, build=None):
        """Any .txt logs present in the build directory are compressed into
        a combined .txt.gz (one per package)."""
        
        if build is None:
            # Use today's build number.
            self.name = 'build' + build_number.todays_build()
            self.num = int(self.name[5:])
        elif type(build) == int:
            self.name = 'build' + str(build)
            self.num = build
        elif type(build) == str:
            if build[:5] != 'build': 
                raise Exception("Event build name must begin with 'build'")
            self.name = build
            self.num = int(build[5:])

        # Where the build is located.
        self.buildDir = os.path.join(config.EVENT_DIR, self.name)
        
        self.packages = ['doomsday', 'fmod']
        self.packageName = {'doomsday': 'Doomsday',
                            'fmod':     'FMOD Ex Audio Plugin'}
        
        # Platforms:  Name                            File ext     sys_id()
        self.oses = [('Windows (x86)',                '.exe',      'win32-32bit'),
                     ('Mac OS X 10.4+ (ppc/i386)',    '.dmg',      'darwin-32bit'),
                     ('Mac OS X 10.6+ (x86_64/i386)', '64bit.dmg', 'darwin-64bit'),
                     ('Ubuntu (x86)',                 'i386.deb',  'linux2-32bit'),
                     ('Ubuntu (x86_64)',              'amd64.deb', 'linux2-64bit')]

        self.platId = {'win32-32bit':  'win-x86',
                       'darwin-32bit': 'mac10_4-x86-ppc',
                       'darwin-64bit': 'mac10_6-x86-x86_64',
                       'linux2-32bit': 'linux-x86',
                       'linux2-64bit': 'linux-x86_64'}

        # Prepare compiler logs present in the build dir.
        self.compress_logs()
        
    def package_type(self, name):
        pkg = self.package_from_filename(name)
        if pkg == 'doomsday':
            return 'distribution'
        else:
            return 'plugin'

    def package_from_filename(self, name):
        if 'fmod' in name:
            return 'fmod'
        else:
            return 'doomsday'        
    
    def os_from_filename(self, name):
        found = None
        for n, ext, ident in self.oses:
            if name.endswith(ext) or ident in name:
                found = (n, ext, ident)
        return found
                
    def version_from_filename(self, name):
        ver = self.extract_version_from_filename(name)
        if not ver and self.package_from_filename(name) == 'doomsday':
            # Fall back to the event version, if it exists.
            ev = self.version()
            if ev: return ev
        return ver

    def extract_version_from_filename(self, name):
        pos = name.find('_')
        if pos < 0: return None
        dash = name.find('-', pos + 1)
        us = name.find('_', pos + 1)
        if dash > 0 and us < 0:
            return name[pos+1:dash]
        elif us > 0 and dash < 0:
            return name[pos+1:us]
        elif dash > 0 and us > 0:
            return name[pos+1:min(dash, us)]
        return name[pos+1:name.rfind('.', pos)]
        
    def tag(self):
        return self.name
        
    def version_base(self):
        ver = self.version()
        if '-' in ver: ver = ver[:ver.find('-')]
        return ver
        
    def version(self):
        fn = self.file_path('version.txt')
        if os.path.exists(fn): return file(fn).read().strip()
        return None
        
    def name(self):
        return self.name
        
    def number(self):
        """Returns the event's build number as an integer."""
        return self.num
        
    def path(self):
        return self.buildDir
        
    def file_path(self, fileName):
        return os.path.join(self.buildDir, fileName)
        
    def clean(self):
        # Make sure we have a clean directory for this build.
        if os.path.exists(self.buildDir):
            # Kill it and recreate.
            shutil.rmtree(self.buildDir, True)
        os.mkdir(self.buildDir)        
        
    def list_package_files(self):
        files = glob.glob(os.path.join(self.buildDir, '*.dmg')) + \
                glob.glob(os.path.join(self.buildDir, '*.exe')) + \
                glob.glob(os.path.join(self.buildDir, '*.deb'))

        return [os.path.basename(f) for f in files]

    def timestamp(self):
        """Looks through the files of the build and returns the timestamp
        for the oldest file."""
        oldest = os.stat(self.buildDir).st_ctime

        for fn in os.listdir(self.buildDir):
            t = os.stat(os.path.join(self.buildDir, fn))
            if int(t.st_mtime) < oldest:
                oldest = int(t.st_mtime)

        return oldest        
        
    def text_timestamp(self):
        return time.strftime(config.RFC_TIME, time.gmtime(self.timestamp()))
    
    def text_summary(self):
        """Composes a textual summary of the event."""
        
        msg = "The build event was started on %s." % self.text_timestamp()

        msg += ' It'

        pkgCount = len(self.list_package_files())

        changesName = self.file_path('changes.html')
        commitCount = 0
        if os.path.exists(changesName):
            commitCount = utils.count_word('<li>', file(changesName).read())
        if commitCount:
            moreThan = ''
            if commitCount == 100: moreThan = 'more than '
            msg += " contains %s%i commits and" % (moreThan, commitCount)

        msg += " produced %i installable binary package%s." % \
            (pkgCount, 's' if (pkgCount != 1) else '')

        return msg
        
    def compress_logs(self):
        if not os.path.exists(self.buildDir): return
        
        """Combines the stdout and stderr logs for a package and compresses
        them with gzip (requires gzip on the system path)."""
        for package in self.packages:
            for osName, osExt, osIdent in self.oses:
                names = glob.glob(self.file_path('%s-*-%s.txt' % (package, osIdent)))
                if not names: continue
                # Join the logs into a single file.
                combinedName = self.file_path('buildlog-%s-%s.txt' % (package, osIdent))
                combined = file(combinedName, 'wt')
                for n in names:
                    combined.write(file(n).read() + "\n\n")
                    # Remove the original log.
                    os.remove(n)
                combined.close()            
                os.system('gzip -f9 %s' % combinedName)        
                
    def download_uri(self, fn):
        # Available on SourceForge?
        if self.number() >= 350 and (fn.endswith('.exe') or fn.endswith('.deb') or fn.endswith('.dmg')):
            if self.release_type() == 'stable':
                return "http://sourceforge.net/projects/deng/files/Doomsday%%20Engine/%s/%s/download" \
                    % (self.version_base(), fn)
            else:
                return "http://sourceforge.net/projects/deng/files/Doomsday%%20Engine/Builds/%s/download" % fn
        # Default to the old location.
        return "%s/%s/%s" % (config.BUILD_URI, self.name, fn)
                
    def compressed_log_filename(self, binaryFn):
        return log_filename(self.package_from_filename(binaryFn), 
                            self.os_from_filename(binaryFn)[2])

    def sort_by_package(self, binaries):
        """Returns the list of binaries sorted by package."""
        pl = []
        for bin in binaries:
            pl.append((self.package_from_filename(bin), bin))
        pl.sort()
        return [bin for pkg, bin in pl]
               
    def html_table_log_issues(self, logName):
        # Link to the compressed log.
        msg = '<td><a href="%s">txt.gz</a>' % self.download_uri(logName)

        # Show a traffic light indicator based on warning and error counts.              
        errors, warnings = utils.count_log_issues(self.file_path(logName))
        form = '<td bgcolor="%s" style="text-align:center;">'
        if errors > 0:
            msg += form % '#ff4444' # red
        elif warnings > 0:
            msg += form % '#ffee00' # yellow
        else:
            msg += form % '#00ee00' # green
        msg += str(errors + warnings)        

        return msg
                
    def html_description(self, encoded=True):
        """Composes an HTML build report."""

        name = self.name
        buildDir = self.buildDir
        oses = self.oses

        msg = '<p>' + self.text_summary() + '</p>'

        # What do we have here?
        files = self.list_package_files()

        # Print out the matrix.
        msg += '<p><table cellspacing="4" border="0">'
        msg += '<tr style="text-align:left;"><th>OS<th>Binary<th>Logs<th>Issues</tr>'

        for osName, osExt, osIdent in oses:
            isFirst = True
            # Find the binaries for this OS.
            binaries = []
            for f in files:
                if self.os_from_filename(f)[2] == osIdent:
                    binaries.append(f)

            if not binaries:
                # Nothing available for this OS.
                msg += '<tr><td>' + osName + '<td>n/a'
                
                # Do we have a log?
                logName = log_filename(self.packages[0], osIdent)
                if os.path.exists(self.file_path(logName)):
                    msg += self.html_table_log_issues(logName)
                
                msg += '</tr>'
                continue

            # List all the binaries. One row per binary.
            for binary in self.sort_by_package(binaries):
                msg += '<tr><td>'
                if isFirst:
                    msg += osName
                    isFirst = False
                msg += '<td>'
                msg += '<a href="%s">%s</a>' % (self.download_uri(binary), binary)

                # Status of the log.
                logName = self.compressed_log_filename(binary)
                if not os.path.exists(self.file_path(logName)):
                    msg += '</tr>'
                    continue                            

                # Link to the compressed log.
                msg += self.html_table_log_issues(logName)

            msg += '</tr>'

        msg += '</table></p>'

        # Changes.
        chgFn = self.file_path('changes.html')
        if os.path.exists(chgFn):
            if utils.count_word('<li>', file(chgFn).read()):
                msg += '<h2>Commits</h2>' + file(chgFn, 'rt').read()

        # Enclose it in a CDATA block if needed.
        if encoded: return '<![CDATA[' + msg + ']]>'    
        return msg
        
    def release_type(self):
        fn = self.file_path('releaseType.txt')
        if os.path.exists(fn):
            return file(fn).read().lower().strip()
        return 'unstable' # Default assumption.
        
    def xml_log(self, logName):
        msg = '<compileLogUri>%s</compileLogUri>' % self.download_uri(logName)
        errors, warnings = utils.count_log_issues(self.file_path(logName))
        msg += '<compileWarnCount>%i</compileWarnCount>' % warnings
        msg += '<compileErrorCount>%i</compileErrorCount>' % errors
        return msg
    
    def release_notes_uri(self, version):
        return "http://dengine.net/dew/index.php?title=Doomsday_version_" + version
        
    def changelog_uri(self, version):
        if self.release_type() == 'stable':
            return self.release_notes_uri(version)
        else:
            return "http://dengine.net/" + self.name          

    def xml_description(self):
        msg = '<build>'
        msg += '<uniqueId>%i</uniqueId>' % self.number()
        msg += '<startDate>%s</startDate>' % self.text_timestamp()
        msg += '<authorName>%s</authorName>' % config.BUILD_AUTHOR_NAME
        msg += '<authorEmail>%s</authorEmail>' % config.BUILD_AUTHOR_EMAIL
        msg += '<releaseType>%s</releaseType>' % self.release_type()
        files = self.list_package_files()
        msg += '<packageCount>%i</packageCount>' % len(files)
        
        # These logs were already linked to.
        includedLogs = []
        
        distribVersion = None
        
        # Packages.
        for fn in files:
            msg += '<package type="%s">' % self.package_type(fn)
            msg += '<name>%s</name>' % self.packageName[self.package_from_filename(fn)]
            msg += '<version>%s</version>' % self.version_from_filename(fn)
            msg += '<platform>%s</platform>' % self.platId[self.os_from_filename(fn)[2]]
            msg += '<downloadUri>%s</downloadUri>' % self.download_uri(fn)
            logName = self.compressed_log_filename(fn)
            if os.path.exists(self.file_path(logName)):
                msg += self.xml_log(logName)
                includedLogs.append(logName)
            msg += '</package>'
            
            if distribVersion is None:
                distribVersion = self.version_from_filename(fn)

        # Any other logs we might want to include?
        for osName, osExt, osIdent in self.oses:
            for pkg in self.packages:
                logName = log_filename(pkg, osIdent)
                if os.path.exists(self.file_path(logName)) and logName not in includedLogs:
                    # Add an entry for this.
                    msg += '<package type="%s">' % self.package_type(logName)
                    msg += '<name>%s</name>' % self.packageName[pkg]
                    if self.version_from_filename(logName):
                        msg += '<version>%s</version>' % self.version_from_filename(logName)
                        if distribVersion is None:
                            distribVersion = self.version_from_filename(logName)
                    msg += '<platform>%s</platform>' % self.platId[osIdent]
                    msg += self.xml_log(logName)
                    msg += '</package>'
                            
        if distribVersion:
            msg += '<releaseNotes>%s</releaseNotes>' % self.release_notes_uri(distribVersion)
            msg += '<changeLog>%s</changeLog>' % self.changelog_uri(distribVersion)
        
        # Commits.
        chgFn = self.file_path('changes.xml')
        if os.path.exists(chgFn):
            msg += file(chgFn, 'rt').read()
                    
        return msg + '</build>'


def find_newest_event():
    newest = None
    for fn in os.listdir(config.EVENT_DIR):
        if fn[:5] != 'build': continue
        ev = Event(fn)
        bt = ev.timestamp()
        if newest is None or newest[0] < bt:
            newest = (bt, ev)

    if newest is None:
        return {'event':None, 'tag':None, 'time':time.time()}
    else:
        return {'event':newest[1], 'tag':newest[1].tag(), 'time':newest[0]}


def find_old_events(atLeastSecs):
    """Returns a list of Event instances."""
    result = []
    now = time.time()
    if not os.path.exists(config.EVENT_DIR): return result
    for fn in os.listdir(config.EVENT_DIR):
        if fn[:5] != 'build': continue
        ev = Event(fn)
        if now - ev.timestamp() >= atLeastSecs:
            result.append(ev)
    return result
    

def find_empty_events(baseDir=None):
    """Returns a list of build directory paths."""
    result = []
    if not baseDir: baseDir = config.EVENT_DIR
    print 'Finding empty subdirs in', baseDir
    for fn in os.listdir(baseDir):
        path = os.path.join(baseDir, fn)
        if os.path.isdir(path):
            # Is this an empty directory?
            empty = True
            for c in os.listdir(path):
                if c != '.' and c != '..':
                    empty = False
                    break
            if empty:
                result.append(path)
    return result


def events_by_time():
    builds = []
    for fn in os.listdir(config.EVENT_DIR):
        if fn[:5] == 'build':
            builds.append((Event(fn).timestamp(), Event(fn)))
    builds.sort()
    builds.reverse()
    return builds

