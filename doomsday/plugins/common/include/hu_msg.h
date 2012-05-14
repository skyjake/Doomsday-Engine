/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * hu_msg.h: Important state change messages.
 */

#ifndef __HUD_MESSAGE_H__
#define __HUD_MESSAGE_H__

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
# include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

typedef enum {
    MSG_CANCEL = -1,
    MSG_NO,
    MSG_YES,
    NUM_MESSAGE_RESPONSES
} msgresponse_t;

typedef int     (C_DECL *msgfunc_t) (msgresponse_t response, void* context);

typedef enum {
    MSG_ANYKEY,
    MSG_YESNO,
    NUM_MESSAGE_TYPES
} msgtype_t;

void            Hu_MsgRegister(void);
void            Hu_MsgInit(void);
void            Hu_MsgShutdown(void);

void            Hu_MsgTicker(void);
int             Hu_MsgResponder(event_t* ev);
void            Hu_MsgDrawer(void);

boolean         Hu_IsMessageActive(void);
boolean         Hu_IsMessageActiveWithCallback(msgfunc_t callback);

void            Hu_MsgStart(msgtype_t type, const char* msg, msgfunc_t callback, void* context);

#endif
