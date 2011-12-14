import os, sys, platform
import glob
import gzip
import codecs
import time
import build_number
import config


def sys_id():
    return "%s-%s" % (sys.platform, platform.architecture()[0])))


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
