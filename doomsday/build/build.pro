# The Doomsday Engine Project
# Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>

TEMPLATE = subdirs

# Update the PK3 files.
system(cd $$PWD/scripts/ && python packres.py $$OUT_PWD/..)
