/** @file consolewidget.h  Console commandline and message history.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DE_CLIENT_CONSOLEWIDGET_H
#define DE_CLIENT_CONSOLEWIDGET_H

#include <de/guiwidget.h>
#include <de/buttonwidget.h>
#include <de/logwidget.h>
#include <de/ipersistent.h>

#include "consolecommandwidget.h"

/**
 * Console command line and message history.
 *
 * ConsoleWidget expects to be bottom-left anchored. It resizes its height
 * automatically. The user can drag the right edge to resize the widget.
 *
 * @ingroup gui
 */
class ConsoleWidget : public de::GuiWidget, public de::IPersistent
{
public:
    DE_AUDIENCE(CommandMode, void commandModeChanged())
    DE_AUDIENCE(GotFocus,    void commandLineGotFocus())

public:
    ConsoleWidget();

    GuiWidget &buttons();

    de::CommandWidget &commandLine();
    de::LogWidget &log();

    const de::Rule &shift();

    bool isLogOpen() const;

    /**
     * Enables or disables the console log background blur.
     *
     * @todo Blurring is presently forcibly disabled when a game is loaded.
     *
     * @param yes  @c true to enable blur, otherwise @c false.
     */
    void enableBlur(bool yes = true);

    // Events.
    void initialize();
    void viewResized();
    void update();
    bool handleEvent(const de::Event &event);

    // Implements IPersistent.
    void operator>>(de::PersistentState &toState) const;
    void operator<<(const de::PersistentState &fromState);

public:
    void openLog();
    void closeLog();
    void closeLogAndUnfocusCommandLine();
    void clearLog();
    void zeroLogHeight();
    void showFullLog();
    void setFullyOpaque();
    void commandLineFocusGained();
    void commandLineFocusLost();
    void focusOnCommandLine();
    void closeMenu();
    void commandWasEntered(const de::String &);
    void copyLogPathToClipboard();

protected:
    void logContentHeightIncreased(int delta);

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_CONSOLEWIDGET_H
