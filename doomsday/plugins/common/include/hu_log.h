/** @file hu_log.h  Game message logging and display.
 *
 * @authors Copyright © 2005-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCOMMON_HULOGDEV_H
#define LIBCOMMON_HULOGDEV_H

#include "hu_lib.h"

/// Maximum number of messages each log will buffer.
#define LOG_MAX_MESSAGES            (8)

/// When a message is first added to the log it may flash depending on
/// the user's configuration. This is the number of tics it takes for
/// each message to animate from flashing to non-flashing.
#define LOG_MESSAGE_FLASHFADETICS   (35)

/// When a message's uptime counter reaches zero it will be removed the
/// from the visible list log. This is the number of tics it takes for
/// each message to animate from visible to non-visible.
#define LOG_MESSAGE_SCROLLTICS      (10)

#ifdef __cplusplus
extern "C" {
#endif

/// To be called to register the console commands and variables of this module.
void UILog_Register(void);

#ifdef __cplusplus
} // extern "C"
#endif

/**
 * UILog. UI widget for player game message logging.
 */

/**
 * @defgroup logMessageFlags  Log Message Flags.
 */
///@{
#define LMF_NO_HIDE             0x1 ///< Always displayed (cannot be hidden by the user).
///@}

typedef struct {
    uint ticsRemain, tics;
    int textMaxLen;
    char *text;
    byte flags;  /// @ref logMessageFlags
} guidata_log_message_t;

typedef struct {
    /// Log message list.
    guidata_log_message_t _msgs[LOG_MAX_MESSAGES];

    /// Number of used messages.
    int _msgCount;

    /// Number of potentially visible messages.
    int _pvisMsgCount;

    /// Index of the next slot to be used in msgs.
    int _nextUsedMsg;
} guidata_log_t;

#ifdef __cplusplus
extern "C" {
#endif

/// Process gametic for this message log.
void UILog_Ticker(uiwidget_t *ob, timespan_t ticLength);

/// Draw this message log widget.
void UILog_Drawer(uiwidget_t *ob, Point2Raw const *offset);

/// Calculate the "physical" dimensions of this widget.
void UILog_UpdateGeometry(uiwidget_t *ob);

/**
 * Empty the message log clearing all messages.
 */
void UILog_Empty(uiwidget_t *ob);

/**
 * Post a message to this log.
 *
 * @param flags  @ref logMessageFlags
 * @param text  Message Text to be posted. Messages may use the same
 *      paramater control blocks as with the engine's Text rendering API.
 */
void UILog_Post(uiwidget_t *ob, byte flags, char const *text);

/**
 * Rewind the message log, making the last few messages visible once again.
 */
void UILog_Refresh(uiwidget_t *ob);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_HULOGDEV_H
