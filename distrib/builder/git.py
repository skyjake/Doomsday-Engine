import os
import builder.config
import subprocess


def run_git(cmdLine, ignoreResult=False):
    result = subprocess.call(cmdLine, shell=True)
    if result and not ignoreResult:
        raise Exception("Failed to run git: " + cmdLine)


def git_checkout(ident):
    """Checkout the branch or tag @a ident from the repository."""
    print 'Checking out %s...' % ident
    os.chdir(builder.config.DISTRIB_DIR)
    run_git("git checkout %s" % ident)


def git_pull():
    """Updates the source with a git pull."""
    print 'Updating source from branch %s...' % builder.config.BRANCH
    os.chdir(builder.config.DISTRIB_DIR)
    run_git("git checkout " + builder.config.BRANCH)
    run_git("git pull")
    
    
def git_tag(tag):
    """Tags the source with a new tag."""
    print 'Tagging with %s...' % tag
    os.chdir(builder.config.DISTRIB_DIR)
    run_git("git tag %s" % tag, ignoreResult=True)
    run_git("git push --tags")
