/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Ker�en <jaakko.keranen@iki.fi>
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

extern id_t clientID;
extern int serverTime;

// Flags for clmobjs.
#define CLMF_HIDDEN			0x01	// Not officially created yet
#define CLMF_UNPREDICTABLE	0x02	// Temporarily hidden (until next delta)
#define CLMF_SOUND			0x04	// Sound is queued for playing on unhide.
#define CLMF_NULLED			0x08	// Once nulled, it can't be updated.
#define CLMF_STICK_FLOOR	0x10	// Mobj will stick to the floor.
#define CLMF_STICK_CEILING	0x20	// Mobj will stick to the ceiling.

typedef struct clmobj_s
{
	struct clmobj_s *next, *prev;
	byte flags;	
	uint time;						// Time of last update.
	int sound;						// Queued sound ID.
	float volume;					// Volume for queued sound.
	mobj_t mo;
} clmobj_t;

typedef struct playerstate_s
{
	clmobj_t	*cmo;
	thid_t		mobjid;
	int			forwardmove;
	int			sidemove;
	int			angle;
	angle_t		turndelta;
	int			friction;
} playerstate_t;

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
