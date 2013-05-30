/** @file consolewidget.h  Console commandline and message history.
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

#ifndef DENG_CLIENT_CONSOLEWIDGET_H
#define DENG_CLIENT_CONSOLEWIDGET_H

#include <QObject>

#include "guiwidget.h"
#include "consolecommandwidget.h"
#include "logwidget.h"

/**
 * Console command line and message history.
 *
 * ConsoleWidget expects to be bottom-left anchored. It resizes its height
 * automatically. The user can drag the right edge to resize the widget.
 */
class ConsoleWidget : public QObject, public GuiWidget
{
    Q_OBJECT

public:
    ConsoleWidget();

    ConsoleCommandWidget &commandLine();
    LogWidget &log();

    // Events.
    bool handleEvent(de::Event const &event);

protected slots:
    void logContentHeightIncreased(int delta);

protected:
    void glInit();
    void glDeinit();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_CONSOLEWIDGET_H
