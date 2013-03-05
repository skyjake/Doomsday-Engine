/** @file statuswidget.h  Widget for status information.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef STATUSWIDGET_H
#define STATUSWIDGET_H

#include <de/shell/TextWidget>
#include <de/shell/Link>

class StatusWidget : public de::shell::TextWidget
{
    Q_OBJECT

public:
    StatusWidget(de::String const &name = "");

    /**
     * Sets the shell Link whose status is to be shown on screen.
     *
     * @param link  Shell connection.
     */
    void setShellLink(de::shell::Link *link);

    void setGameState(de::String const &mode, de::String const &rules, de::String const &mapId);

    void draw();

public slots:
    void refresh();
    void linkConnected();
    void linkDisconnected();

private:
    DENG2_PRIVATE(d)
};

#endif // STATUSWIDGET_H
