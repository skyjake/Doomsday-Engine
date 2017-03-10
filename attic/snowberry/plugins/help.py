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

import string, time, os, webbrowser
import language, events, ui, host
import sb.aodb as ao
import sb.addon
import sb.profdb as pr
import sb.confdb as st
import sb.util.dialog


# An instance of the message timer.
helpTextTimer = ui.Timer('show-help-text-now')

# A widget will be created during initialization.
helpText = None

# This is true if the help panel shouldn't be updated at the moment.
helpDisabled = False

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
    
    ui.addMenuCommand(ui.MENU_HELP, 'open-documentation', pos=0)
    
    try:
        helpArea = ui.getArea(ui.HELP)
    except KeyError:
        # The Help area does not exist. We don't have much to do here.
        events.addCommandListener(handleCommand, ['open-documentation'])
        return
        
    helpArea.setExpanding(True)
    helpArea.setWeight(1)

    # Create a HTML text widget.
    global helpText
    helpArea.setBorder(3)
    helpText = helpArea.createFormattedText()

    # Unfreeze the help text after a minor delay. This'll make the app
    # init a bit smoother.
    helpText.freeze()
    helpTextTimer.start(1000)

    # Set parameters suitable for the logo.
    helpArea.setWeight(0)
    helpArea.setBorder(0)

    # Register a listener.
    events.addCommandListener(handleCommand, ['help-addon-mode-brief',
                                              'help-addon-mode-detailed',
                                              'freeze', 'unfreeze',
                                              'open-documentation'])

    events.addNotifyListener(handleNotify, ['show-help-text-now',
                                            'init-done',
                                            'active-profile-changed',
                                            'tab-selected',
                                            'addon-list-selected',
                                            'maps-list-selected',
                                            'focus-changed',
                                            'value-changed',
                                            'language-changed'] )


def showLogo(doShow=True):
    """Show or hide the Snowberry logo in the bottom of the help panel.

    @param doShow True, if the logo should be shown.
    """
    global logo

    try:
        helpArea = ui.getArea(ui.HELP)
    except KeyError:
        # No Help area.
        return

    if not logo and doShow:
        logo = helpArea.createImage('help-logo')
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
    global helpDisabled
    
    if event.hasId('help-addon-mode-brief'):
        detailedAddonMode = False
        if currentAddon:
            showAddonInfo(currentAddon)

    elif event.hasId('help-addon-mode-detailed'):
        detailedAddonMode = True
        if currentAddon:
            showAddonInfo(currentAddon)

    elif event.hasId('freeze'):
        helpDisabled = True

    elif event.hasId('unfreeze'):
        helpDisabled = False
        updateHelpText()    
        
    elif event.hasId('open-documentation'):
        webbrowser.open_new(language.translate('documentation-url'))
            

def handleNotify(event):
    """This is called when somebody sends a notification.

    @param event A events.Notify object.
    """
    if event.hasId('show-help-text-now'):
        helpText.unfreeze()    
        return
    
    if helpDisabled:
        return
    
    if event.hasId('init-done'):
        setField(FIELD_MAIN, language.translate('help-welcome'))
        setField(FIELD_COMMAND, language.translate('help-command-defaults'))
        updateHelpText()    

    elif event.hasId('show-help-text-now'):
        helpText.unfreeze()
                        
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

    elif event.hasId('addon-list-selected') or \
         event.hasId('maps-list-selected'):
        # Display information about the selected addon in the panel.
        try:
            showAddonInfo(ao.get(event.getSelection()))
        except KeyError:
            # It wasn't an addon.
            pass

    elif event.hasId('focus-changed'): 
        try:
            setting = st.getSetting(event.getFocus())
            showSettingInfo(setting)
        except KeyError:
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
          'cellspacing="0" cellpadding="6"><tr>' + \
          '<td align="left">'
    msg += string.join(helpFields, "<p>") + "</table></html>"
    helpText.setText(msg)
    showLogo()

    # No longer showing a setting.
    global currentSetting
    currentSetting = None


# Background color for table header entries.
bgColor = "#E8E8E8"
tableRow = '<tr>'
tableBegin = '<table width="100%" cellspacing="1" cellpadding="2">'
tableEnd = '</table><p>'


def entryHeader(text, span=1):
    htmlSize = st.getSystemInteger('style-html-size') - 1
    width = ''
    return '<td %s bgcolor="%s" valign="top" colspan="%i"><font size="%i">%s</font>' % (width, bgColor, span, htmlSize, text)

def entryContent(text, span=1):
    htmlSize = st.getSystemInteger('style-html-size') - 1
    return '<td valign="top" colspan="%i"><font size="%i">%s</font>' \
           % (span, htmlSize, text)


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
    msg += tableBegin

    msg += '<tr><td colspan="2"><b><font size="+1">' + \
           language.translate(ident) + \
           '</font></b>'

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

    for type, label in [(sb.addon.EXCLUDES, 'help-addon-excludes'),
                        (sb.addon.REQUIRES, 'help-addon-requires'),
                        (sb.addon.PROVIDES, 'help-addon-provides'),
                        (sb.addon.OFFERS, 'help-addon-offers')]:
        # Make a copy of the list so we can modify it.
        keywords = [kw for kw in addon.getKeywords(type)]

        # In the Brief mode, hide the identifier of the addon in the
        # Provides field.
        if not detailedAddonMode and type == sb.addon.PROVIDES:
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

    msg += tableEnd

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
    msg += '<br><table bgcolor="' + bgColor + '" width="100%"><tr><td>' + \
           '<font size="-1"><tt>' + \
           setting.getCommandLine(prof) + '</tt></font></table><p>'

    fromDefaults = False

    if prof.getValue(ident, False) == None:
        fromDefaults = True
        # The value comes from the default profile.
        msg += language.expand(
            language.translate('help-value-from-defaults'),
            pr.getDefaults().getName()) + '<p>'

    def valueToText(v):
        if language.isDefined(v):
            return language.translate(v)
        else:
            return v

    msg += tableBegin

    # The current value of the setting.
    value = prof.getValue(ident)
    if value:
        #msg += '<b>%s:</b><br>' % language.translate('help-value-current')
        #msg += valueToText(value.getValue())
        #msg += '<p>'

        msg += tableRow + \
               entryHeader(language.translate('help-value-current')) + \
               entryContent(valueToText(value.getValue()))

    # Min and max for the range and slider settings.
    if setting.getType() == 'slider' or setting.getType() == 'range':
        #msg += '<b>' + language.translate('help-value-min') + ':</b><br>' + \
        #       str(setting.getMinimum())
        #msg += '<p><b>' + language.translate('help-value-max') + \
        #       ':</b><br>' + str(setting.getMaximum())
        #msg += '<p>'

        msg += tableRow + entryHeader(language.translate('help-value-min')) + \
               entryContent(str(setting.getMinimum())) + \
               tableRow + entryHeader(language.translate('help-value-max')) + \
               entryContent(str(setting.getMaximum()))

    # The default.
    if prof.getValue(ident, False) != None and prof is pr.getDefaults():
        if setting.getDefault() != None:
            #msg += '<b>%s:</b><br>' % language.translate('help-value-default')
            #msg += valueToText(str(setting.getDefault()))
            #msg += '<p>'

            msg += tableRow + \
                   entryHeader(language.translate('help-value-default')) + \
                   entryContent(valueToText(str(setting.getDefault())))
            
    elif not fromDefaults and prof is not pr.getDefaults():
        defValue = pr.getDefaults().getValue(ident)
        if defValue:
            #msg += '<b>%s:</b><br>' % language.translate('help-value-default')
            #msg += valueToText(defValue.getValue())
            #msg += '<p>'

            msg += tableRow + \
                   entryHeader(language.translate('help-value-default')) + \
                   entryContent(valueToText(defValue.getValue()))

    msg += tableEnd

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
