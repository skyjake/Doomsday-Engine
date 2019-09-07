/** @file hu_msg.h  Important game state change messages.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_HUD_MESSAGE_H
#define LIBCOMMON_HUD_MESSAGE_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MSG_CANCEL = -1,
    MSG_NO,
    MSG_YES,
    NUM_MESSAGE_RESPONSES
} msgresponse_t;

typedef int     (C_DECL *msgfunc_t) (msgresponse_t response, int userValue, void *userPointer);

typedef enum {
    MSG_ANYKEY,
    MSG_YESNO,
    NUM_MESSAGE_TYPES
} msgtype_t;

/**
 * Called during the PreInit of each game during start up.
 * Register Cvars and CCmds for the important messages.
 */
void Hu_MsgRegister(void);

/**
 * Called during init.
 */
void Hu_MsgInit(void);

/**
 * Called during engine shutdown.
 */
void Hu_MsgShutdown(void);

/**
 * Updates on Game Tick.
 */
void Hu_MsgTicker(void);

/**
 * If an "any key" message is active, respond to the event.
 */
int Hu_MsgResponder(event_t *ev);

/**
 * Draw any active message.
 */
void Hu_MsgDrawer(void);

dd_bool Hu_IsMessageActive(void);
dd_bool Hu_IsMessageActiveWithCallback(msgfunc_t callback);

/**
 * Begin a new game state message/question.
 */
void Hu_MsgStart(msgtype_t type, const char *msg, msgfunc_t callback, int userValue, void *userPointer);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_HUD_MESSAGE_H
