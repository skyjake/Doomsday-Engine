import os
import builder.config


def git_checkout(ident):
    """Checkout the branch or tag @a ident from the repository."""
    print 'Checking out %s...' % ident
    os.chdir(builder.config.DISTRIB_DIR)
    os.system("git checkout %s" % ident)


def git_pull():
    """Updates the source with a git pull."""
    print 'Updating source...'
    os.chdir(builder.config.DISTRIB_DIR)
    os.system("git checkout master")
    os.system("git pull")
    
    
def git_tag(tag):
    """Tags the source with a new tag."""
    print 'Tagging with %s...' % tag
    os.chdir(builder.config.DISTRIB_DIR)
    os.system("git tag %s" % tag)
    os.system("git push --tags")
