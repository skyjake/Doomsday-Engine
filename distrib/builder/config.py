import os
import sys


def get_arg(label):
    """Find the value for the command line option @a label."""
    if label in sys.argv:
        return sys.argv[sys.argv.index(label) + 1]
    return None


BUILD_URI = "http://code.iki.fi/builds"
RFC_TIME = "%a, %d %b %Y %H:%M:%S +0000"
EVENT_DIR = os.path.join(os.environ['HOME'], 'BuildMaster')
DISTRIB_DIR = '.'
APT_REPO_DIR = ''

val = get_arg('--distrib')
if val is not None: DISTRIB_DIR = val

val = get_arg('--events')
if val is not None: EVENT_DIR = val

val = get_arg('--apt')
if val is not None: APT_REPO_DIR = val
