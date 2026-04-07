/** @file chatwidget.h  Specialized HudWidget for player messaging.
 *
 * @authors Copyright © 2005-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_UI_CHATWIDGET_H
#define LIBCOMMON_UI_CHATWIDGET_H

#include <de/error.h>
#include <de/string.h>

#include "hu_lib.h"

enum menucommand_e;

/**
 * Specialized HudWidget for staging of (player) chat messages.
 *
 * @par Destination numbering:
 * - @c 0= All players (i.e., a "global" message).
 * - @c 1..NUMTEAMS All players with the team number @c n-1.
 *
 * @ingroup ui
 */
class ChatWidget : public HudWidget
{
public:
    /// An invalid/unknown destination was referenced. @ingroup errors
    DE_ERROR(DestinationError);

public:
    ChatWidget(int player);
    virtual ~ChatWidget();

    /**
     * Returns @c true if the widget is responsive to input.
     */
    bool isActive() const;

    /**
     * Change the "active" state (i.e., accepting of user input) of the widget. When
     * the widget is activated, the destination for subsequent messages is set to
     * @em "global" and the staged message is cleared.
     *
     * @post The "chat" binding context is (de)activated as necessary.
     */
    void activate(bool yes = true);

    /**
     * Returns the current destination for chat messages encoded as a number.
     */
    int destination() const;

    /**
     * Change the destination i.e., would-be recipient(s) for subsequent chat
     * messages.
     *
     * @param newDestination  New destination to be applied.
     */
    void setDestination(int newDestination);

    int handleEvent(const event_t &ev);
    int handleMenuCommand(menucommand_e cmd);

    void updateGeometry();
    void draw(const de::Vec2i &offset = de::Vec2i()) const;

public:  // Outbound message staging: ---------------------------------------------
    /**
     * Clear the chat message being staged.
     */
    void messageClear();

    /**
     * Append the given @a text to the chat message being staged.
     * @param text  Text to be appended.
     */
    void messageAppend(const de::String &text);

    /**
     * Convenient method of replacing the chat message being staged.
     * @param text  Text to be copied.
     */
    inline void messageCopy(const de::String &text) {
        messageClear();
        messageAppend(text);
    }

    /**
     * Convenient method of appending the text string associated with the given
     * @a macroId (if found) to the chat message being staged. If no macro exists
     * with this id then nothing happens.
     */
    inline void messageAppendMacro(int macroId) {
        messageAppend(findMacro(macroId));
    }

    /**
     * Convenient method of replacing the chat message being staged with the text
     * content of the given @a macroId. If no macro exists with this id then the
     * staged message will be cleared.
     */
    inline void messageCopyMacro(int macroId) {
        messageClear();
        messageAppendMacro(macroId);
    }

    /**
     * Returns an immutable pointer to a copy of the chat message being staged in
     * text form.
     */
    de::String messageAsText() const;

public:
    /**
     * Prepare the macro-indexed chat messages from the game config. To be called
     * once definitions have been read to prepare for using macros.
     */
    static void loadMacros();

    /**
     * Given a macro identifier lookup the associated macro string. There are ten
     * user-configurable macros bound to ids in the range [0..9].
     */
    static de::String findMacro(int macroId);

    /**
     * Register the console commands and variables of this module.
     */
    static void consoleRegister();

private:
    DE_PRIVATE(d)
};

#endif  // LIBCOMMON_UI_CHATWIDGET_H
