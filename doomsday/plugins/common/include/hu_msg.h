/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

#ifndef __HUD_MESSAGES_H__
#define __HUD_MESSAGES_H__

#if  __DOOM64TC__
# include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

void        HUMsg_Register(void);

void        HUMsg_Drawer(void);
void        HUMsg_Ticker(void);
void        HUMsg_Start(void);
void        HUMsg_Init(void);

void        HUMsg_PlayerMessage(player_t *plr, char *message, int tics,
                                boolean noHide, boolean yellow);

#endif
