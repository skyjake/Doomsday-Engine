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

/*
 * Cheat code checking.
 */

#ifndef __M_CHEAT__
#define __M_CHEAT__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "h_stat.h"

void        cht_GodFunc(player_t *player);
void        cht_NoClipFunc(player_t *player);
void        cht_SuicideFunc(player_t *player);
boolean     cht_Responder(event_t *ev);

#endif
