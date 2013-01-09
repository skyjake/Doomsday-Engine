# The Doomsday Engine Project: Server Shell
# Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is distributed under the GNU General Public License
# version 2 (or, at your option, any later version). Please visit
# http://www.gnu.org/licenses/gpl.html for details.
#
# -=-
#
# The Shell is an application for running and monitoring a Doomsday server.
# It is composed out of a shared library with common functionality, a GUI
# application based on Qt, and a command line utility with text-mode console
# connection to the server.

TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS += libshell shell-gui
