/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * cl_def.h: Client Definitions
 */

#ifndef __DOOMSDAY_CLIENT_H__
#define __DOOMSDAY_CLIENT_H__

#include "p_mobj.h"

#define	SHORTP(x)		(*(short*) (x))
#define	USHORTP(x)		(*(unsigned short*) (x))

extern ident_t clientID;
extern int serverTime;
extern boolean handshakeReceived;
extern int gameReady;
extern boolean netLoggedIn;
extern boolean clientPaused;

void Cl_InitID(void);
void Cl_CleanUp();
void Cl_GetPackets(void);
void Cl_Ticker(timespan_t time);
int Cl_GameReady();
void Cl_SendHello(void);

#endif
