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

## @file tab10_summary.py Summary Tab
##
## This plugin implements the Summary tab.  It displays a basic
## summarization of the settings in the active profile.
##
## This is the default tab.  It is displayed when Snowberry is
## started.

import string
import ui, events
import sb.profdb as pr
import sb.confdb as st
import sb.aodb as ao
import sb.widget.text as wt
import language
from ui import ALIGN_HORIZONTAL

SUMMARY = 'tab-summary'

# If true, the summary tab won't be updated on notifications.
summaryDisabled = False


def init():
    "Create the Summary page."
    area = ui.createTab(SUMMARY)

    # Partition the area into various info fields.
    #bottomHalf = area.createArea(border = 4)
    #topHalf = area.createArea(border = 4)
    
    # The profile title and the description.
    #area.setWeight(1)
    #global description
    #description = area.createFormattedText()

    # The bottom half contains a summary of the active profile's
    # settings.
    area.setBorder(6)
    area.setWeight(0)

    global titleLabel
    area.setBorder(16, ui.BORDER_NOT_BOTTOM)
    titleLabel = area.createText('')
    titleLabel.setTitleStyle()
    
    area.setBorder(2, ui.BORDER_ALL)
    area.setExpanding(False)
    area.addSpacer()
    area.setExpanding(True)

    area.setBorder(16, ui.BORDER_NOT_TOP)
    area.createLine()

    area.setBorder(8, ui.BORDER_ALL)
    addonInfo = area.createArea(alignment=ALIGN_HORIZONTAL)
    area.setWeight(0)
    systemInfo = area.createArea(alignment=ALIGN_HORIZONTAL)
    valuesInfo = area.createArea(alignment=ALIGN_HORIZONTAL)

    # Information about addons selected into the profile.
    addonInfo.setBorder(4, ui.BORDER_LEFT_RIGHT)
    addonInfo.setWeight(1)
    addonInfo.createText('loaded-addons', ':', align=wt.Text.RIGHT).setBoldStyle()
    addonInfo.setWeight(2)

    global addonListing
    addonListing = addonInfo.createText()

    # Values for the most important system settings.
    systemInfo.setBorder(4, ui.BORDER_LEFT_RIGHT)
    systemInfo.setWeight(1)
    systemInfo.createText('system-settings', ':', align=wt.Text.RIGHT).setBoldStyle()
    systemInfo.setWeight(2)

    global systemSummary
    systemSummary = systemInfo.createText()
    systemSummary.setText('800x600; run in window')

    # Information about settings that have a value in the profile.
    valuesInfo.setBorder(4, ui.BORDER_LEFT_RIGHT)
    valuesInfo.setWeight(1)
    valuesInfo.createText('custom-settings', ':', align=wt.Text.RIGHT).setBoldStyle()
    valuesInfo.setWeight(2)

    global valuesListing
    valuesListing = valuesInfo.createText()

    # Listen for active profile changes.
    events.addNotifyListener(notifyHandler, ['active-profile-changed',
                                             'value-changed',
                                             'addon-attached',
                                             'addon-detached'])
    events.addCommandListener(commandHandler, ['freeze', 'unfreeze'])


def commandHandler(event):
    global summaryDisabled
    
    if event.hasId('freeze'):
        summaryDisabled = True

    elif event.hasId('unfreeze'):
        summaryDisabled = False


def notifyHandler(event):
    if summaryDisabled:
        return
    
    if event.hasId('active-profile-changed'):
        p = pr.getActive()
        titleLabel.setText(p.getName())

        # Update the summary entries.
        updateSummary(p)

        # Change to the Summary tab automatically.
        if event.hasId('active-profile-changed') and \
               st.getSystemBoolean('summary-profile-change-autoselect'):
            if event.wasChanged(): 
                # The profile did actually change.
                ui.selectTab(SUMMARY)

    elif event.hasId('value-changed') or \
         event.hasId('addon-attached') or \
         event.hasId('addon-detached'):
        # Resummarize due to a changed value of a setting.
        updateSummary(pr.getActive())


def updateSummary(profile):
    """Update the fields on the summary tab.  Each tab summarises
    certain aspects of the profile's settings.

    @param profile A profiles.Profile object.  The values will be
    taken from this profile.
    """
    # Addon listing.
    summary = []
    usedAddons = profile.getUsedAddons()
    ao.sortIdentifiersByName(usedAddons)

    usedAddons = filter(lambda a: ao.get(a).getBox() == None, usedAddons)

    for addon in usedAddons:
        # Don't let the list get too long; there is no scrolling, the
        # extra text will just get clipped...
        if len(summary) < 8:
            summary.append(language.translate(addon))

    if len(summary) == 0:
        summary = ['-']

    if len(summary) < len(usedAddons):
        # Indicate that all are not shown.
        summary[-1] += ' ...'
        
    addonListing.setText(string.join(summary, "\n"))
    addonListing.resizeToBestSize()

    # Values defined in the profile.
    summary = []

    # These are displayed in another summary field or shouldn't be
    # shown at all.
    ignoredValues = ['window-size', 'window-width', 'window-height',
                     'color-depth', 'run-in-window']

    for value in profile.getAllValues():

        # Don't let the list get too long; there is no scrolling, the
        # extra text will just get clipped...
        if len(summary) >= 10:
            summary.append('...')
            break

        # Many settings are ignored in the summary.
        try:
            setting = st.getSetting(value.getId())
            if (setting.getGroup() == 'general-options' or
                (setting.getGroup() == 'game-options' and 'server' not in setting.getId())):
                continue
        except KeyError:
            # This isn't even a valid setting!
            continue

        if value.getId() in ignoredValues:
            continue
        
        msg = language.translate(value.getId())
        if language.isDefined(value.getValue()):
            if value.getValue() != 'yes':
                msg += ' = ' + language.translate(value.getValue()) 
        else:
            msg += ' = ' + value.getValue() 
        summary.append(msg)

    if len(summary) == 0:
        summary = ['-']
        
    valuesListing.setText(string.join(summary, "\n"))
    valuesListing.resizeToBestSize()
    
    # The system summary shows the basic display settings.
    summary = []

    # Begin with the resolution.
    value = profile.getValue('window-size')
    if value and value.getValue() != 'window-size-custom':
        summary.append(language.translate(value.getValue()))
    else:
        w = profile.getValue('window-width')
        h = profile.getValue('window-height')
        if w and h:
            summary.append(w.getValue() + ' x ' + h.getValue())

    # Windowed mode is a special case.
    value = profile.getValue('run-in-window')
    if value and value.getValue() == 'yes':
        summary.append(language.translate('summary-run-in-window'))
    else:
        value = profile.getValue('color-depth')
        if value:
            summary.append(language.translate('summary-' + \
                                              value.getValue()))

    systemSummary.setText(string.join(summary, '\n'))
    systemSummary.resizeToBestSize()

    ui.getArea(SUMMARY).updateLayout()
