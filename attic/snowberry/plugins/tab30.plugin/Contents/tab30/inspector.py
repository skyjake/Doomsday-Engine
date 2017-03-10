# -*- coding: iso-8859-1 -*-
# $Id: main.py 4303 2007-01-01 19:06:40Z skyjake $
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

## @file inspector.py Addon Inspector Dialog

import os, time, string
import paths, events, ui, language
import sb.util.dialog
import sb.widget.button as wg
import sb.widget.list
import sb.profdb as pr
import sb.confdb as st
import sb.aodb as ao
import sb.addon


def run(addon):
    """Show a dialog box that contains a lot of information (all there
    is to know) about the addon.

    @param An addons.Addon object.
    """
    ident = addon.getId()
    
    dialog, area = sb.util.dialog.createButtonDialog(
        'addon-inspector-dialog',
         ['ok'], 'ok', size=(570, 450))

    msg = ""

    msg += '<h3>' + language.translate(ident) + '</h3>'
    if language.isDefined(ident + '-readme'):
        msg += "<p>" + language.translate(ident + '-readme')

    def makeField(header, content):
        return '<tr><td width="20%" bgcolor="#E8E8E8" align="right"><b>' + header + \
               '</b></td><td width="80%">' + content + '</td></tr>'

    beginTable = '<p><table width="100%" border=0 cellpadding=6 cellspacing=0>'
    endTable = '</table>'

    #
    # General Information
    #
    msg += beginTable
    msg += makeField('Summary:', language.translate(ident + '-summary', '-'))
    msg += makeField('Version:', language.translate(ident + '-version', '-'))
    msg += makeField('Last Modified:',
                     time.strftime("%a, %d %b %Y %H:%M:%S",
                                   time.localtime(addon.getLastModified())))
    msg += makeField('Author(s):', language.translate(ident + '-author', '-'))
    msg += makeField('Contact:', language.translate(ident + '-contact', '-'))
    msg += makeField('Copyright:',
                     language.translate(ident + '-copyright', '-'))
    msg += makeField('License:', language.translate(ident + '-license', '-'))
    msg += makeField('Category:', addon.getCategory().getPath())
    msg += makeField('Format:', language.translate(addon.getType()))
    msg += makeField('Identifier:', addon.getId())
    msg += endTable

    #
    # Raw Dependencies
    #
    msg += '<h3>Dependencies</h3>' + beginTable
    allDeps = []
    
    # Excluded categories.
    dep = []
    for category in addon.getExcludedCategories():
        dep.append(category.getPath())
    allDeps.append(('Excluded Categories:', dep))

    # Keywords.
    for type, label in [(sb.addon.EXCLUDES, 'Excludes:'),
                        (sb.addon.REQUIRES, 'Requires:'),
                        (sb.addon.PROVIDES, 'Provides:'),
                        (sb.addon.OFFERS, 'Offers:')]:
        # Make a copy of the list so we can modify it.
        dep = []
        for kw in addon.getKeywords(type):
            dep.append(kw)
        allDeps.append((label, dep))

    # Create a table out of each dependency type.
    for heading, listing in allDeps:
        msg += makeField(heading, string.join(listing, '<br>'))

    msg += endTable

    #
    # Content Analysis
    # 
    msg += '<h3>Contents</h3>' + beginTable
    msg += makeField('Content Path:', addon.getContentPath())
    msg += endTable
    
    #msg += "<p>format-specific data"
    #msg += "<br>content analysis, size"
    #msg += "<br>list of files, if a bundle"

    #msg += '<h3>Identifiers</h3>'
    #msg += "<br>all internal identifiers used by the addon"

    #msg += '<h3>Metadata</h3>'
    #msg += "<br>metadata analysis, source"

    text = area.createFormattedText()
    text.setMinSize(500, 200)
    text.setText(msg)
    dialog.run()        


