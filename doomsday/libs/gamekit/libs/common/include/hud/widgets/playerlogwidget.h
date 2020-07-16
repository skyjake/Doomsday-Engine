/** @file playerlogwidget.h  Specialized HudWidget for logging player messages.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 id Software, Inc.
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBCOMMON_UI_PLAYERLOGWIDGET_H
#define LIBCOMMON_UI_PLAYERLOGWIDGET_H

#include <de/string.h>
#include "hud/hudwidget.h"

/// Maximum number of messages each log will buffer.
#define LOG_MAX_ENTRIES             ( 8 )

/// When a message is first added to the log it may flash depending on
/// the user's configuration. This is the number of tics it takes for
/// each message to animate from flashing to non-flashing.
#define LOG_MESSAGE_FLASHFADETICS   ( 35 )

/// When a message's uptime counter reaches zero it will be removed the
/// from the visible list log. This is the number of tics it takes for
/// each message to animate from visible to non-visible.
#define LOG_MESSAGE_SCROLLTICS      ( 10 )

/**
 * Specialized HudWidget for logging player messages.
 *
 * @ingroup ui
 *
 * @todo Separate underlying log from this widget (think MVC).
 */
class PlayerLogWidget : public HudWidget
{
public:
    /**
     * Models a single entry in the player message log.
     */
    struct LogEntry
    {
        bool justAdded = false;
        bool dontHide  = false;
        de::duint ticsRemain = 0;
        de::duint tics       = 0;
        de::String text;
    };

    PlayerLogWidget(int player);
    virtual ~PlayerLogWidget();

    /**
     * Empty the log clearing all log entries.
     */
    void clear();

    /**
     * Rewind the log, making the last few entries visible again.
     */
    void refresh();

    /**
     * Post a entry to the log.
     *
     * @param flags    @ref logMessageFlags
     * @param message  Text to be posted. The message may contain encoded parameters
     * intended for Doomsday's @em FR text rendering API.
     */
    void post(int flags, const de::String &mesage);

    void tick(timespan_t elapsed);
    void updateGeometry();
    void draw(const de::Vec2i &offset = de::Vec2i());

    /**
     * Register the console commands and variables of this module.
     */
    static void consoleRegister();

private:
    DE_PRIVATE(d)
};

#endif  // LIBCOMMON_UI_PLAYERLOGWIDGET_H
