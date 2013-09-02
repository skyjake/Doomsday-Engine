/** @file taskbarwidget.h
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_TASKBARWIDGET_H
#define DENG_CLIENT_TASKBARWIDGET_H

#include <QObject>
#include <de/Action>

#include "GuiWidget"
#include "consolewidget.h"
#include "buttonwidget.h"

/**
 * Task bar that acts as the primary UI element of the client's UI.
 *
 * @ingroup gui
 */
class TaskBarWidget : public GuiWidget
{
    Q_OBJECT

public:
    TaskBarWidget();

    ConsoleWidget &console();
    ConsoleCommandWidget &commandLine();
    ButtonWidget &logoButton();

    bool isOpen() const;
    de::Rule const &shift();

    // Events.
    void viewResized();
    void drawContent();
    bool handleEvent(de::Event const &event);

public slots:
    void open();
    void openAndPauseGame();
    void close();
    void openMainMenu();
    void closeMainMenu();
    void unloadGame();
    void showAbout();
    void showUpdaterSettings();
    void showVideoSettings();
    void showAudioSettings();

signals:
    void opened();
    void closed();

protected:
    void glInit();
    void glDeinit();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_TASKBARWIDGET_H
