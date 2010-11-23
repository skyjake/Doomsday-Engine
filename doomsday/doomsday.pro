TEMPLATE = subdirs
SUBDIRS = libdeng2 tests server
tests.depends = libdeng2
server.depends = libdeng2
