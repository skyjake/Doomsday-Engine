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

## @file logger.py Warning/Error Logger
##
## This module collections warnings and errors and displays them when
## asked to.

import sys, traceback
import language, string


# An array with the currently collection issues.
allIssues = []

# Levels of severity.
LOW = 0
MEDIUM = 1
HIGH = 2


def add(severity, identifier, *args):
    """Add a new issue to the list of issues waiting to be displayed.

    @param severity The severity of the issue.  One of the following
    constants:
    - LOW 
    - MEDIUM
    - HIGH

    @param identifier Identifier of the issue.

    @param args An array of arguments.  These will be used to expand
    placeholders in the translated version of the identifier.
    """
    global allIssues

    allIssues.append((severity, identifier, list(args)))
    
    
def immediate(severity, identifier, *args):
    """Show a logger issue immediately. See logger.add() for the params."""
    
    add(severity, identifier, *args)
    show()


def show():
    """Show all the issues in a dialog.  The list of issues will be
    automatically cleared.  If there are no issues to report, nothing
    is done."""

    global allIssues

    if len(allIssues) == 0:
        # Nothing to report.
        return

    import sb.util.dialog
    dialog, area = sb.util.dialog.createButtonDialog(
        'issue-dialog',
        ['ok'], 'ok')

    message = area.createFormattedText()
    message.setMinSize(500, 300)

    msg = ''

    bgColor = ['#90E090', '#FFFF80', '#E05050']
    fgColor = ['black', 'black', 'white']
    severeName = ['Note', 'Warning', 'Error']

    isFirst = True

    # Compose a HTML formatted version of each issue.
    for severe in [HIGH, MEDIUM, LOW]:
        # Show the important issues first.
        for issue in allIssues:
            if issue[0] != severe:
                continue

            if not isFirst:
                msg += '<hr>'
            isFirst = False

            msg += '<table width="100%" border=0 cellspacing=3 cellpadding=4>'
            msg += '<tr><td bgcolor="%s" width="20%%" align=center>' % \
                   bgColor[issue[0]]
            msg += '<font color="%s"><b>' % fgColor[issue[0]] + \
                   severeName[issue[0]] + '</b></font>'
            msg += '<td width="80%"><h3>' + language.expand(
                language.translate(issue[1]), *issue[2])
            msg += '</h3>' + language.expand(
                language.translate(issue[1] + '-text'), *issue[2])
            msg += '</table>'

    message.setText(msg)
    dialog.run()
        
    # Only show each issue once.
    allIssues = []


def formatTraceback():
    """Make a rough HTML formatting of the system traceback.   
    @return Traceback as a string of HTML text."""    
    
    str = string.join(traceback.format_exception_only(sys.exc_type, sys.exc_value)) + '\n'
    
    str += string.join(traceback.format_list(
                       traceback.extract_tb(sys.exc_traceback)), '<br>')
    str = str.replace('\n', '<br>')
    str = str.replace(' ', '&nbsp;')
    return '<font size="-3"><tt>' + str + '</tt></font>'
