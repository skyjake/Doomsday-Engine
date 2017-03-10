# -*- coding: iso-8859-1 -*-
# $Id$
# Snowberry: Extensible Launcher for the Doomsday Engine
#
# Copyright (C) 2004, 2005
#   Jaakko Keränen <jaakko.keranen@iki.fi>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not: http://www.opensource.org/

## @file snowberry.py The main module.
##
## This module is the main module for the entire launcher.

import language, ui, plugins, sb.profdb


def main():
    # Load all plugins.
    plugins.loadAll()

    # Load all profiles from disk.
    sb.profdb.restore()

    # Prepare window layout and contents.
    ui.prepareWindows()

    # Start the main loop.
    #import profile
    #profile.run('ui.startMainLoop()', 'prof.log')
    ui.startMainLoop()

#profile.run('main()', 'report.log')
main()
