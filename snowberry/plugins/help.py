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

import string, time
import events, ui, language
import profiles as pr
import addons as ao
import settings as st

# A widget will be created during initialization.
helpText = None

# The Snowberry logo in the bottom of the help panel.
logo = None

# The normal help text is composed of several fields.
FIELD_MAIN = 0
FIELD_CURRENT_TAB = 1
FIELD_CURRENT_SETTING = 2
FIELD_COMMAND = 3

helpFields = ['', '', '', '']

detailedAddonMode = False
currentAddon = None
currentSetting = None


def init():
    "Create the HTML text widget into the help area."
    helpArea = ui.getArea(ui.Area.HELP)
    helpArea.setExpanding(True)
    helpArea.setWeight(1)

    # Create a HTML text widget.
    global helpText
    helpArea.setBorder(3)
    helpText = helpArea.createFormattedText()

    # Set parameters suitable for the logo.
    helpArea.setWeight(0)
    helpArea.setBorder(0)

    # Register a listener.
    events.addCommandListener(handleCommand)

    events.addNotifyListener(handleNotify)


def showLogo(doShow=True):
    """Show or hide the Snowberry logo in the bottom of the help panel.

    @param doShow True, if the logo should be shown.
    """
    global logo

    helpArea = ui.getArea(ui.Area.HELP)

    if not logo and doShow:
        logo = helpArea.createImage('snowberry')
        helpArea.updateLayout()

    elif logo and not doShow:
        helpArea.destroyWidget(logo)
        logo = None
        helpArea.updateLayout()
    

def handleCommand(event):
    """This is called when somebody sends a command.

    @param event A events.Command object.
    """
    global detailedAddonMode
    
    if event.hasId('help-addon-mode-brief'):
        detailedAddonMode = False
        if currentAddon:
            showAddonInfo(currentAddon)

    elif event.hasId('help-addon-mode-detailed'):
        detailedAddonMode = True
        if currentAddon:
            showAddonInfo(currentAddon)


def handleNotify(event):
    """This is called when somebody sends a notification.

    @param event A events.Notify object.
    """
    if event.hasId('init-done'):
        setField(FIELD_MAIN, language.translate('help-welcome'))
        setField(FIELD_COMMAND, language.translate('help-command-defaults'))
        updateHelpText()                     
    
    elif event.hasId('active-profile-changed'):
        if pr.getActive() is pr.getDefaults():
            setField(FIELD_COMMAND,
                     language.translate('help-command-defaults'))
        else:
            setField(FIELD_COMMAND, language.translate('help-command'))

    elif event.hasId('tab-selected'):
        setField(FIELD_MAIN, '')
        setField(FIELD_CURRENT_TAB,
                 language.translate('help-' + event.getSelection()))

    elif event.hasId('addon-tree-selected'):
        # Display information about the selected addon in the panel.
        try:
            showAddonInfo(ao.get(event.getSelection()))
        except:
            # It wasn't an addon.
            pass

    elif event.hasId('focus-changed'):
        try:
            setting = st.getSetting(event.getFocus())
            showSettingInfo(setting)
        except:
            # It was likely not a setting id.
            pass

    elif event.hasId('value-changed'):
        if currentSetting and event.getSetting() == currentSetting.getId():
            showSettingInfo(currentSetting)

    elif event.hasId('language-changed'):
        pass
        

def setField(which, text):
    global helpFields
    helpFields[which] = text
    updateHelpText()


def updateHelpText():
    """Update the help text with the standard fields.  The values of
    the fields depend on the currently selected tabs and settings.
    """
    msg = '<html><table width="100%" height="100%" border="0" ' + \
          'cellspacing="0" cellpadding="0"><tr>' + \
          '<td align="center">'
    msg += string.join(helpFields, "<p>") + "</table></html>"
    helpText.setText(msg)
    showLogo()

    # No longer showing a setting.
    global currentSetting
    currentSetting = None


def showAddonInfo(addon):
    """Display a summary of an addon in the help panel.

    @param addon An addons.Addon object.
    """
    global currentAddon, currentSetting

    # No longer showing a setting.
    currentSetting = None

    # The current addon that is displayed in the help.
    currentAddon = addon

    oneLiners = (detailedAddonMode == False)
    tableRow = '<tr>'
    bgColor = "#E8E8E8"
    ident = addon.getId()
    
    # Begin the page with a table that contains the basic info.
    msg = '<html>'

    def optLink(id, isLink):
        if isLink:
            return '<a href="%s">%s</a>' % (id, language.translate(id))
        else:
            return language.translate(id)

    # The detail selection links.
    msg += '<center><font size="-1">'
    msg += optLink('help-addon-mode-brief', detailedAddonMode) + ' | '
    msg += optLink('help-addon-mode-detailed', not detailedAddonMode)
    msg += '</font></center><br>'

    # Summary table.
    msg += '<table width="100%" cellspacing="1" cellpadding="2">'

    msg += '<tr><td colspan="2"><b><font size="+1">' + \
           language.translate(ident) + \
           '</font></b>'

    def entryHeader(text, span=1):
        #if span == 1:
        #    width = 'width="10%"'
        #else:
        width = ''
        return '<td %s bgcolor="%s" valign="top" colspan="%i"><font size="-1">%s</font>' % (width, bgColor, span, text)

    def entryContent(text, span=1):
        return '<td valign="top" colspan="%i"><font size="-1">%s</font>' % (span, text)

    def makeField(msg, fieldId, oneLiner=True):
        title = language.translate('help-addon-' + fieldId)
        contentId = ident + '-' + fieldId

        text = ''
        if language.isDefined(contentId):
            # This information has been defined.
            text = language.translate(contentId)
        elif detailedAddonMode:
            # Force all fields to show.
            text = '-'

        if text:
            if oneLiner:
                msg += tableRow + entryHeader(title) + entryContent(text)
            else:
                msg += tableRow + entryHeader(title, 2) + \
                       tableRow + entryContent(text, 2)

        return msg

    msg = makeField(msg, 'version')

    if detailedAddonMode:
        # Get the last modification time from the addon.
        modTime = time.localtime(addon.getLastModified())
        msg += tableRow + entryHeader(language.translate('help-addon-date')) \
               + entryContent(time.strftime("%d %b %Y", modTime))
    
    msg = makeField(msg, 'summary', oneLiners)

    if detailedAddonMode:
        msg = makeField(msg, 'contact', oneLiners)
        msg = makeField(msg, 'author', oneLiners)
        msg = makeField(msg, 'copyright', oneLiners)
        msg = makeField(msg, 'license', oneLiners)

    # Dependencies.
    deps = []
    
    exCats = addon.getExcludedCategories()
    if len(exCats):
        deps.append((language.translate('help-addon-excluded-categories'),
                     map(lambda c: language.translate(c.getLongId()), exCats)))

    for type, label in [(ao.Addon.EXCLUDES, 'help-addon-excludes'),
                        (ao.Addon.REQUIRES, 'help-addon-requires'),
                        (ao.Addon.PROVIDES, 'help-addon-provides'),
                        (ao.Addon.OFFERS, 'help-addon-offers')]:
        # Make a copy of the list so we can modify it.
        keywords = [kw for kw in addon.getKeywords(type)]

        # In the Brief mode, hide the identifier of the addon in the
        # Provides field.
        if not detailedAddonMode and type == ao.Addon.PROVIDES:
            keywords.remove(addon.getId())            
        
        if len(keywords) > 0:
            # Include this in the info.
            def xlate(key):
                if ao.exists(key):
                    return '<a href="%s">%s</a>' % \
                           (key, language.translate(key))
                else:
                    return key
            deps.append((language.translate(label), map(xlate, keywords)))

    if len(deps):
        # Print a dependencies table.
        msg += tableRow + entryHeader(
            language.translate('help-addon-dependencies'), 2)

        content = ""

        for dep in deps:
            if dep is not deps[0]:
                content += '<br>'
            content += "<b>" + dep[0] + ":</b><ul><li>"
            content += string.join(dep[1], '<li>')
            content += '</ul>'

        msg += tableRow + entryContent(content, 2)            

    msg += '</table><p>'

    # Inside a box?
    if addon.getBox():
        box = addon.getBox()
        msg += language.expand(
            language.translate('help-addon-part-of-box'),
            box.getId(), language.translate(box.getId())) + '<p>'

    # The overview.
    if language.isDefined(ident + '-readme'):
        msg += language.translate(ident + '-readme')

    msg += '</html>'

    helpText.setText(msg)
    showLogo(False)


def showSettingInfo(setting):
    """Display information about the focused setting in the help
    panel.

    @param setting A settings.Setting object.
    """
    # Values will be taken from the active profile.
    prof = pr.getActive()

    # The identifier of the setting.
    ident = setting.getId()
    
    msg = '<html>'

    # The name of the setting.
    msg += '<b><font size="+1">' + language.translate(ident) + \
           '</font></b>'

    # The command line option.
    msg += '<br><table bgcolor="#E8E8E8" width="100%"><tr><td>' + \
           '<font size="-1"><tt>' + \
           setting.getCommandLine(prof) + '</tt></font></table><p>'

    if prof.getValue(ident, False) == None:
        # The value comes from the default profile.
        msg += language.expand(
            language.translate('help-value-from-defaults'),
            pr.getDefaults().getName()) + '<p>'

    def valueToText(v):
        if language.isDefined(v):
            return language.translate(v)
        else:
            return v

    # The current value of the setting.
    value = prof.getValue(ident)
    if value:
        msg += '<b>%s:</b><br>' % language.translate('help-value-current')
        msg += valueToText(value.getValue())
        msg += '<p>'

    # Min and max for the range and slider settings.
    if setting.getType() == 'slider' or setting.getType() == 'range':
        msg += '<b>' + language.translate('help-value-min') + ':</b> ' + \
               str(setting.getMinimum())
        msg += ' <b>' + language.translate('help-value-max') + ':</b> ' + \
               str(setting.getMaximum())
        msg += '<p>'

    # The default.
    if prof.getValue(ident, False) != None and prof is pr.getDefaults():
        if setting.getDefault() != None:
            msg += '<b>%s:</b><br>' % language.translate('help-value-default')
            msg += valueToText(str(setting.getDefault()))
            msg += '<p>'
    elif prof is not pr.getDefaults():
        defValue = pr.getDefaults().getValue(ident)
        if defValue:
            msg += '<b>%s:</b><br>' % language.translate('help-value-default')
            msg += valueToText(defValue.getValue())
            msg += '<p>'

    # The help text of this setting.
    helpId = ident + '-help'
    if language.isDefined(helpId):
        msg += language.translate(helpId)

    msg += '</html>'

    # Display the setting information in the help panel.
    helpText.setText(msg)
    showLogo(False)

    # The info will be automatically updated in case it changes while
    # displayed.
    global currentSetting
    currentSetting = setting
