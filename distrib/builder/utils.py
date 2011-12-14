import os
import glob
import gzip
import time
import build_number
import config


def remote_copy(src, dst):
    dst = dst.replace('\\', '/')
    os.system('scp %s %s' % (src, dst))  


def collated(s):
    s = s.strip()
    while s[-1] == '.':
        s = s[:-1]
    return s


def todays_build_tag():
    now = time.localtime()
    return 'build' + build_number.todays_build()


def build_timestamp(tag):
    """Looks through the files of a given build and returns the timestamp
    for the oldest file."""
    path = os.path.join(config.EVENT_DIR, tag)
    oldest = os.stat(path).st_ctime

    for fn in os.listdir(path):
        t = os.stat(os.path.join(path, fn))
        if int(t.st_ctime) < oldest:
            oldest = int(t.st_ctime)

    return oldest        


def list_package_files(name):
    buildDir = os.path.join(config.EVENT_DIR, name)

    files = glob.glob(os.path.join(buildDir, '*.dmg')) + \
            glob.glob(os.path.join(buildDir, '*.exe')) + \
            glob.glob(os.path.join(buildDir, '*.deb'))

    return [os.path.basename(f) for f in files]


def find_newest_build():
    newest = None
    for fn in os.listdir(config.EVENT_DIR):
        if fn[:5] != 'build': continue
        bt = build_timestamp(fn)
        if newest is None or newest[0] < bt:
            newest = (bt, fn)
    if newest is None:
        return {'tag': None, 'time': time.time()}
    return {'tag': newest[1], 'time': newest[0]}


def find_old_builds(atLeastSecs):
    result = []
    now = time.time()
    if not os.path.exists(config.EVENT_DIR): return result
    for fn in os.listdir(config.EVENT_DIR):
        if fn[:5] != 'build': continue
        bt = build_timestamp(fn)
        if now - bt >= atLeastSecs:
            result.append({'time':bt, 'tag':fn})
    return result


def find_empty_events(baseDir=None):
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


def builds_by_time():
    builds = []
    for fn in os.listdir(config.EVENT_DIR):
        if fn[:5] == 'build':
            builds.append((build_timestamp(fn), fn))
    builds.sort()
    builds.reverse()
    return builds


def aptrepo_by_time(arch):
    files = []
    for fn in os.listdir(os.path.join(config.APT_REPO_DIR, 'dists/unstable/main/binary-' + arch)):
        if fn[-4:] == '.deb':
            files.append(fn)
    return files

def count_log_word(fn, word):
    txt = unicode(gzip.open(fn).read(), 'latin1').lower()
    pos = 0
    count = 0
    while True:
        pos = txt.find(unicode(word), pos)
        if pos < 0: break 
        if txt[pos-1] not in ['/', '\\'] and txt[pos+len(word)] != 's' and \
            txt[pos-11:pos] != 'shlibdeps: ':
            count += 1            
        pos += len(word)
    return count


def count_log_status(fn):
    """Returns tuple of (#warnings, #errors) in the fn."""
    return (count_log_word(fn, 'error'), count_log_word(fn, 'warning'))    


def count_word(word, inText):
    pos = 0
    count = 0
    while True:
        pos = inText.find(word, pos)
        if pos < 0: break
        count += 1
        pos += len(word)
    return count
