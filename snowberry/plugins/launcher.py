# -*- coding: iso-8859-1 -*-
# $Id$
# Snowberry: Extensible Launcher for the Doomsday Engine
#
# Copyright (C) 2004, 2011
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

## @file launcher.py Launcher Plugin
##
## Implements the user interface for launching the game.  Handles
## confliction resolution using dialogs.
##
## A cleaner implementation might define its own dialog class for the
## resolver, derived from AreaDialog.

# TODO: Implement a timer service so wx isn't needed here.

import os, string, time, subprocess
import ui, host
import sb.util.dialog
import events
import paths
import language
import sb.widget.button as wg
import sb.profdb as pr
import sb.confdb as st
import sb.aodb as ao
import sb.expressions as ex


class MessageTimer (ui.Timer):
    """The message timer will clear the launch message when the 
    timer expires."""

    def __init__(self):
        ui.Timer.__init__(self)
        
    def expire(self):
        """Called when the timer expires."""
        
        # Clear the launch message.
        setLaunchMessage('')


# An instance of the message timer.
launchTextTimer = MessageTimer()


class ResolveStatus:
    def __init__(self):
        self.dialog = None
        self.area = None
        self.problem = None
        self.profile = None
        
    def dontUse(self, addon):
        self.profile.dontUseAddon(addon.getId())
        # Also remove from the set of addons.
        if addon in self.addons:
            self.addons.remove(addon)

    def createConflictButtons(self, addonName):
        self.area.setWeight(0)
        self.choices = []
        self.choices.append(
            resolving.area.createRadioButton('', True, True))
        self.choices.append(
            resolving.area.createRadioButton('', False))
            
        self.choices[0].setText(language.expand(
            language.translate('conflict-choice1'), addonName))
            
        self.choices[1].setText(language.expand(
            language.translate('conflict-choice2'), addonName))


def init():
    """Create the Play button."""
    
    area = ui.getArea(ui.COMMAND)
    area.addSpacer()

    global logFileName
    logFileName = os.path.join(paths.getUserPath(paths.RUNTIME),
                               "Conflicts.log")

    global launchText
    launchText = area.createText('') #launch-message')

    global playButton
    playButton = area.createButton('play', wg.Button.STYLE_DEFAULT)

    global resolving
    resolving = ResolveStatus()

    playButton.setPopupMenu(['view-command-line'])

    # Register for listening to notifications.
    events.addNotifyListener(handleNotify, ['active-profile-changed'])

    # Register for listening to commands.
    events.addCommandListener(handleCommand, ['play', 'view-command-line',
                                              'continue'])
    
    # Commands for the popup menu.
    ui.addMenuCommand(ui.MENU_APP, 'quit', group=ui.MENU_GROUP_APP)
    ui.addMenuCommand(ui.MENU_PROFILE, 'play', group=ui.MENU_GROUP_LAUNCH)
    ui.addMenuCommand(ui.MENU_TOOLS, 'view-command-line', group=ui.MENU_GROUP_LAUNCH)


def handleNotify(event):
    if event.hasId('active-profile-changed'):
        # Disable or enable controls based on which profile is selected.
        if pr.getActive() is pr.getDefaults():
            playButton.disable()
            ui.disableMenuCommand('play')
            ui.disableMenuCommand('view-command-line')
        else:
            playButton.enable()
            ui.enableMenuCommand('play')
            ui.enableMenuCommand('view-command-line')


def handleCommand(event):
    if pr.getActive() is pr.getDefaults():
        return

    if event.hasId('play'):
        # Launch the game with the active profile.
        startGame(pr.getActive())

    elif event.hasId('continue') and resolving:
        continueResolving()

    elif event.hasId('view-command-line'):
        # Generate a command line and display it in a dialog.
        options = generateOptions(pr.getActive())
        if options == None:
            return

        # Escape any angle brackets.
        options = options.replace('<', '&lt;')
        options = options.replace('>', '&gt;')
        options = options.replace(' -', '<br>-')

        # Highlight all the options with a bold font.
        pos = 0
        while pos < len(options):
            if options[pos:pos + 5] == '<br>-' or \
                   pos == 0 and options[pos] == '-':
                options = options[:pos] + '<b>' + options[pos:]
                pos += 5
                while pos < len(options) and \
                      options[pos] not in string.whitespace:
                    pos += 1
                options = options[:pos] + '</b>' + options[pos:]
            pos += 1

        dialog, area = sb.util.dialog.createButtonDialog(
            'view-command-line-dialog',            
            ['ok'], 'ok')

        msg = area.createFormattedText()
        msg.setMinSize(500, 400)
        msg.setText('<tt>' + options + '</tt>')
        dialog.run()        


def setLaunchMessage(text):
    """Launch message is displayed next to the Play button."""
    
    launchText.setText(text)
    launchText.resizeToBestSize()
    ui.getArea(ui.COMMAND).updateLayout()
    
    if text:
        # Clear the message after a short delay.
        launchTextTimer.start(4000)


def startGame(profile):
    """Start the game using the specified profile.

    @param profile A profiles.Profile object.
    """
    setLaunchMessage(language.translate('launch-message'))
    
    # Broadcast a notification about the launch of a game.

    # Locate the paths and binaries.  Most of these are configured in
    # the .conf files.
    engineBin = st.getSystemString('doomsday-binary')

    options = generateOptions(profile)
    if options == None:
        return

    # Put the response file in the user's runtime directory.
    responseFile = os.path.join(paths.getUserPath(paths.RUNTIME), 'Options.rsp')

    file(responseFile, 'w').write(options + "\n")

    # Execute the command line.
    if host.isWindows():
        if '-dedicated' in options:
            batFile = os.path.join(paths.getUserPath(paths.RUNTIME), 'launch.bat')
            bat = file(batFile, 'wt')
            print >> bat, '@ECHO OFF'
            print >> bat, 'ECHO Launching Doomsday...'
            curDir = os.getcwd()
            print >> bat, 'cd', paths.quote(curDir)
            print >> bat, "%s @%s" % (paths.quote(engineBin), paths.quote(responseFile))
            bat.close()
            engineBin = batFile
            spawnFunc = spawnWithTerminal
        else:
            spawnFunc = os.spawnv
    elif host.isMac() and '-dedicated' in options:
        # On the Mac, we'll tell Terminal to run it.
        osaFile = os.path.join(paths.getUserPath(paths.RUNTIME), 'Launch.scpt')
        scpt = file(osaFile, 'w')
        print >> scpt, 'tell application "Terminal"'
        print >> scpt, '    activate'
        def q1p(s): return '\\\"' + s + '\\\"'
        def q2p(s): return '\\\\\\\"' + s + '\\\\\\\"'
        curDir = os.getcwd()
        print >> scpt, "    do script \"cd %s; %s @%s\"" % \
            (q1p(curDir), engineBin.replace(' ', '\\\\ '), responseFile.replace(' ', '\\\\ '))
        print >> scpt, 'end tell'
        scpt.close()
        engineBin = osaFile
        spawnFunc = spawnWithTerminal
    elif host.isUnix() and '-dedicated' in options:
        shFile = os.path.join(paths.getUserPath(paths.RUNTIME), 'launch.sh')
        sh = file(shFile, 'w')
        print >> sh, '#!/bin/sh'
        print >> sh, "cd %s" % (paths.quote(os.getcwd()))
        print >> sh, "%s @%s" % (paths.quote(engineBin), responseFile.replace(' ', '\\ '))
        sh.close()
        os.chmod(shFile, 0744)
        engineBin = paths.quote(shFile)
        spawnFunc = spawnWithTerminal
    else:
        spawnFunc = os.spawnvp

    spawnFunc(os.P_NOWAIT, engineBin, [engineBin, '@' + responseFile])

    # Shut down if the configuration settings say so.
    value = profile.getValue('quit-on-launch')
    if not value or value.getValue() == 'yes':
        # Quit Snowberry.
        events.sendAfter(events.Command('quit'))


def spawnWithTerminal(wait, launchScript, arguments):
    term = st.getSystemString('system-terminal').split(' ')
    #print term + [launchScript]
    subprocess.Popen(term + [launchScript])

    #if host.isWindows():
    #    spawn = os.spawnv
    #else:
    #    spawn = os.spawnvp
    #spawn(wait, term[0], term + [launchScript])    


def generateOptions(profile):
    """Generate a text string of all the command line options used
    when launching a game.

    @param profile A profiles.Profile object.  The values of settings
    are retrieved from here.

    @return All the options in a single string.  Returns None if there
    is an unresolved addon conflict.
    """
    clearLog()
    
    # Determine which addons are in use.  The final list of addons
    # includes all the addons that must be loaded at launch (defaults;
    # required parts of boxes).
    usedAddonIds = profile.getFinalAddons()
    usedAddons = map(lambda id: ao.get(id), usedAddonIds)

    # Form the command line.
    cmdLine = []
    
    # Determine the settings that apply to the components and
    # addons.
    effectiveSettings = st.getCompatibleSettings(profile)

    # Are there any system-specific options defined?
    if st.isDefined('common-options'):
        cmdLine.append(ex.evaluate(st.getSystemString('common-options'), None))

    # All profiles use the same runtime directory.
    if st.isDefined('doomsday-base'):
        basePath = os.path.abspath(st.getSystemString('doomsday-base'))
        cmdLine.append('-basedir ' + paths.quote(basePath))

    userPath = paths.getUserPath(paths.RUNTIME)
    cmdLine.append('-userdir ' + paths.quote(userPath))

    # Determine the components used by the profile.
    for compId in profile.getComponents():
        # Append the component's options to the command line as-is.
        cmdLine.append( st.getComponent(compId).getOption() )

    # Resolve conflicting addons by letting the user choose between
    # them.
    if not resolveAddonConflicts(usedAddons, profile):
        # Launch aborted!
        return None

    # Get the command line options for loading all the addons.
    for addon in usedAddons:
        params = addon.getCommandLine(profile).strip()
        if params:
            cmdLine.append(params)

    # Update IDs of used addons.
    usedAddonsId = map(lambda a: a.getId(), usedAddons)

    # Get the command line options from each setting.
    for setting in effectiveSettings:
        # If the setting's required addons are not being loaded, skip
        # this setting.
        skipThis = False
        for req in setting.getRequiredAddons():
            if req not in usedAddonIds:
                skipThis = True
                break

        if skipThis:
            continue
        
        # All settings can't be used at all times.  In
        # case of a conflict, some of the settings are ignored.
        if setting.isEnabled(profile):
            params = setting.getCommandLine(profile).strip()
            if params:
                cmdLine.append(params)

    # Send a launch options notification.  This is done so that
    # plugins are able to observe/modify the list of launch options.
    events.send(events.LaunchNotify(cmdLine))

    return string.join(cmdLine, ' ')


def clearLog():
    try:
        os.remove(logFileName)
    except:
        pass
        
    global logFile
    logFile = file(logFileName, 'w')

    logFile.write(st.getSystemString('snowberry-title') + ' ' +
                  st.getSystemString('snowberry-version') + "\n")
    logFile.write("Launching on " +
                  time.strftime('%a, %d %b %Y %H:%M:%S') + "\n")
    

def log(problem):
    """Print a message about conflict resolution in the log."""
    
    global logFile

    problemName = problem[0]

    logFile.write("\n%s:\n" % problemName)

    if problemName == 'override':
        source = problem[1]
        targets = problem[2]
        logFile.write("  preferred addon = " + source.getId() + "\n")
        logFile.write("  not loaded = [\n")
        for target, key in targets:
            logFile.write("    %s (because of %s)\n" % (target.getId(), key))
        logFile.write("  ]\n")

    elif problemName == 'provide-conflict':
        logFile.write("  conflicting providers = [\n")
        for source, target, key in problem[1]:
            logFile.write("    %s / %s (because of %s)\n" % (
                source.getId(), target.getId(), key))
        logFile.write("  ]\n")

    elif problemName == 'missing-requirements':
        logFile.write("  %s requires the following = [\n" % problem[1].getId())
        for key in problem[2]:
            logFile.write("    %s\n" % key)
        logFile.write("  ]\n")
        
    elif problemName == 'exclusion-by-category':
        logFile.write("  addon = " + problem[1].getId() + "\n")
        logFile.write("  excluded addons = [\n")
        for a in problem[2]:
            logFile.write("    %s\n" % a.getId())
        logFile.write("  ]\n")

    elif problemName == 'exclusion-by-value':
        logFile.write("  addon = " + problem[1].getId() + "\n")
        logFile.write("  conflicting values = [\n")
        for v in problem[2]:
            logFile.write("    %s\n" % v)
        logFile.write("  ]\n")

    elif problemName == 'exclusion-by-keyword':
        logFile.write("  addon = " + problem[1].getId() + "\n")
        logFile.write("  conflicted by = [\n")
        for a, key in problem[2]:
            logFile.write("    %s (because of %s)\n" % (a.getId(), key))
        logFile.write("  ]\n")

    logFile.flush()
    

def getNextProblem(addons, profile):
    """Gets a list of problems in the set of addons.  Automatically
    resolves overrides.

    @return An array of conflict descriptions.
    """

    problems = []

    # Resolve overrides immediately.  They don't cause any changes to
    # the profile.
    for prob in ao.findConflicts(addons, profile):
        if prob[0] == 'override':
            log(prob)

            # Remove the overrided addons from the set.
            for a, key in prob[2]:
                if a in addons:
                    addons.remove(a)
        else:
            problems.append(prob)

    if len(problems) > 0:
        return problems[0]
    else:
        return None


def resolveAddonConflicts(addons, profile):
    """Resolve all conflicts in the set of the addons.

    @param addons An array of Addon objects.  This array is modified
    in this function.

    @param profile A profiles.Profile object.

    @return Possible return values:
    - True: the conflicts were resolved
    - False: the launch should be cancelled
    """
    global logFile
    global resolving

    # Print the load order into the log.
    logFile.write("Addons to load:\n")
    for a in addons:
        logFile.write("  " + a.getId() + "\n")

    # The resolving process uses a high-first priority.
    addons.reverse()

    #print "CONFLICTS:"
    #print ao.resolveConflicts(addons, profile)

    problem = getNextProblem(addons, profile)

    # We'll loop here until no more problems are found.
    if problem:
        # There are problems to resolve.  Let's open the Conflict
        # Wizard, which will take care of the issue.
        resolving = ResolveStatus()
        resolving.profile = profile
        resolving.dialog, resolving.area = sb.util.dialog.createButtonDialog(
            'conflict-wizard',
            ['cancel', 'continue'], 'continue')

        # Initialize the wizard with the first problem.
        createResolver(problem, addons)

        if resolving.dialog.run() == 'cancel':
            return False

        # We're done.
        resolving = None

    # Restore the original order so that Doomsday gets the highest
    # priority last.
    addons.reverse()

    logFile.write("\nResolved addons:\n")
    for a in addons:
        logFile.write("  " + a.getId() + "\n")
    logFile.flush()
    return True


def createResolver(problem, addons):
    """Create the necessary widgets in the area to resolve the problem.

    @param area An ui.Area object.  Will be cleared.

    @param problem The problem description as returned by
    addons.findConflicts.

    @param addons Set of Addon objects.  This should be modified
    according to the user's selections.
    """
    global resolving

    # Set the general parameters of the resolver.
    resolving.problem = problem
    resolving.addons = addons
    
    resolving.area.clear()
    resolving.area.setWeight(2)

    message = resolving.area.createFormattedText()
    message.setMinSize(250, 250)

    resolving.area.setWeight(1)

    # Enter this problem into the log.
    log(problem)
    
    # The name of the problem.
    problemName = problem[0]

    if problemName == 'exclusion-by-category':
        # The list of excluded addons. 
        names = map(lambda a: language.translate(a.getId()), problem[2])
        names.sort()

        msg = "<ul><li>" + string.join(names, '<li>') + "</ul>"
    
        message.setText(language.expand(
            language.translate('category-conflict-message'),
            language.translate(problem[1].getId()), msg))

        # Create the radio buttons.
        resolving.createConflictButtons(language.translate(problem[1].getId()))

    elif problemName == 'exclusion-by-keyword':
        msg = '<ul>'

        for addon, key in problem[2]:
            msg += '<li>'
            if addon.getId() != key:
                msg += key + " in "
            msg += language.translate(addon.getId())

        msg += '</ul>'

        message.setText(language.expand(
            language.translate('keyword-conflict-message'),
            language.translate(problem[1].getId()), msg))

        # Create the radio buttons.
        resolving.createConflictButtons(language.translate(problem[1].getId()))

    elif problemName == 'exclusion-by-value':
        # The list of excluded values.
        names = []
        for v in problem[2]:
            if language.isDefined(v):
                names.append(language.translate(v))
            else:
                names.append(v)
        msg = "<ul><li>" + string.join(names, '<li>') + "</ul>"
        message.setText(language.expand(
            language.translate('value-conflict-message'),
            language.translate(problem[1].getId()), msg))            

    elif problemName == 'provide-conflict':
        # Collect the conflicting addons into a list.
        resolving.conflicted = []
        keys = []
        for a, b, key in problem[1]:

            if a not in resolving.conflicted:
                resolving.conflicted.append(a)
                
            if b not in resolving.conflicted:
                resolving.conflicted.append(b)
                
            if key not in keys:
                keys.append(key)

        #ao.sortIdentifiersByName(conflicting)

        message.setText(language.expand(
            language.translate('provide-conflict-message'),
            "<ul><li>" + string.join(keys, '<li>') + "</ul>"))

        resolving.list = resolving.area.createList('')
        for a in resolving.conflicted:
            resolving.list.addItem(a.getId())

    elif problemName == 'missing-requirements':

        resolving.conflicted = problem[1]

        res = "<p><ul>"
        res += "<li>" + string.join(problem[2], '<li>') + "</ul>"

        msg = language.expand(
            language.translate('missing-requirements-message'),
            language.translate(problem[1].getId()))
        
        message.setText(msg + res)

        resolving.list = None
        
    resolving.area.updateLayout()


def continueResolving():
    """The user pressed the Continue button in the resolver."""

    # Assume that the currently active profile is the one used for
    # launching.
    profile = resolving.profile

    if resolving.problem[0] == 'provide-conflict':
        # Remove all but the selected addon.
        sel = resolving.list.getSelectedItem()
        if not len(sel):
            return

        # Something was actually selected.  Remove the other
        # addons form the profile.
        for addon in resolving.conflicted:
            if addon.getId() != sel:
                resolving.dontUse(addon)

        problemSolved()

    elif resolving.problem[0] == 'missing-requirements':
        # Remove the addon from the profile.
        addon = resolving.conflicted
        
        profile.dontUseAddon(addon.getId())
        
        if addon in resolving.addons:
            resolving.addons.remove(addon)

        problemSolved()
        
    elif resolving.problem[0] == 'exclusion-by-category':
        if resolving.choices[0].isChecked():
            # Remove the excluded addons.
            for addon in resolving.problem[2]:
                resolving.dontUse(addon)
        else:
            # Remove the addon causing the conflict.
            resolving.dontUse(resolving.problem[1])
        
        problemSolved()

    elif resolving.problem[0] == 'exclusion-by-value':
        resolving.dontUse(resolving.problem[1])
        problemSolved()

    elif resolving.problem[0] == 'exclusion-by-keyword':
        if resolving.choices[0].isChecked():
            # Remove the excluded addons.
            for addon, key in resolving.problem[2]:
                resolving.dontUse(addon)
        else:
            # Remove the addon causing the conflict.
            resolving.dontUse(resolving.problem[1])
        
        problemSolved()


def problemSolved():
    """Called when the current problem has been solved in the wizard."""

    global resolving
    
    problem = getNextProblem(resolving.addons, resolving.profile)

    if problem:
        createResolver(problem, resolving.addons)

    else:
        # We're done.  Close the resolver wizard.
        resolving.dialog.close('ok')
        resolving = None
