/** @file statuswidget.h  Widget for status information.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/term/widget.h>
#include <doomsday/network/link.h>

class StatusWidget : public de::term::Widget
{
public:
    StatusWidget(const de::String &name = de::String());

    /**
     * Sets the shell Link whose status is to be shown on screen.
     *
     * @param link  Shell connection.
     */
    void setShellLink(network::Link *link);

    void setGameState(const de::String &mode, const de::String &rules, const de::String &mapId);

    void draw();

private:
    DE_PRIVATE(d)
};

#endif // STATUSWIDGET_H
