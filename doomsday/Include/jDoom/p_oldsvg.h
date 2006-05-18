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

#ifndef __P_V19_SAVEG__
#define __P_V19_SAVEG__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

void            SV_v19_LoadGame(char *filename);

// Persistent storage/archiving.
// These are the load / save game routines.
void            P_v19_UnArchivePlayers(void);
void            P_v19_UnArchiveWorld(void);
void            P_v19_UnArchiveThinkers(void);
void            P_v19_UnArchiveSpecials(void);

#endif
