/** @file consolecommandwidget.h  Text editor with a history buffer.
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

#ifndef DENG_CLIENT_CONSOLECOMMANDWIDGET_H
#define DENG_CLIENT_CONSOLECOMMANDWIDGET_H

#include "lineeditwidget.h"

/**
 * Text editor with a history buffer. Entered commands are executed as console
 * commands. Uses the console Lexicon for word completion.
 *
 * Whenever the current game changes, the lexicon is automatically updated to
 * include the terms of the loaded game.
 *
 * @ingroup gui
 */
class ConsoleCommandWidget : public LineEditWidget
{
    Q_OBJECT

public:
    ConsoleCommandWidget(de::String const &name = "");

    // Events.
    void focusGained();
    void focusLost();
    bool handleEvent(de::Event const &event);

signals:
    void gotFocus();
    void lostFocus();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_CONSOLECOMMANDWIDGET_H
