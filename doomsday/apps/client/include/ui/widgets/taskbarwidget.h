/** @file taskbarwidget.h
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

#ifndef DE_CLIENT_TASKBARWIDGET_H
#define DE_CLIENT_TASKBARWIDGET_H

#include <de/action.h>
#include <de/guiwidget.h>
#include <de/buttonwidget.h>

#include "consolewidget.h"

/**
 * Task bar that acts as the primary UI element of the client's UI.
 *
 * @ingroup gui
 */
class TaskBarWidget : public de::GuiWidget
{
public:
    DE_AUDIENCE(Open,  void taskBarOpened())
    DE_AUDIENCE(Close, void taskBarClosed())

public:
    TaskBarWidget();

    ConsoleWidget &    console();
    de::CommandWidget &commandLine();
    de::ButtonWidget & logoButton();

    bool isOpen() const;
    const de::Rule &shift();

    // Events.
    void initialize();
    //void viewResized();
    void update();
    void drawContent();
    bool handleEvent(const de::Event &event);

public:
    void open();
    void openAndPauseGame();
    void close();
    void openConfigMenu();
    void closeConfigMenu();
    void openMainMenu();
    void closeMainMenu();
    void openMultiplayerMenu();
    void unloadGame();
    void showAbout();
    void showOrHideHome();
    void connectToServerManually();
    void showTutorial();
    void openPackagesSidebar();
#if defined (DE_HAVE_UPDATER)
    void showUpdaterSettings();
#endif

protected:
    void updateCommandLineLayout();

protected:
    void glInit();
    void glDeinit();

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_TASKBARWIDGET_H
