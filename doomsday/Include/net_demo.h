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
 * net_demo.h: Demos
 */

#ifndef __DOOMSDAY_DEMO_H__
#define __DOOMSDAY_DEMO_H__

#include "con_decl.h"

extern int demotic, playback;

void Demo_Init(void);
void Demo_Ticker(timespan_t time);

boolean Demo_BeginRecording(char *filename, int playernum);
void Demo_StopRecording(int playernum);
void Demo_PauseRecording(int playernum);
void Demo_ResumeRecording(int playernum);
void Demo_WritePacket(int playernum);
void Demo_BroadcastPacket(void);
void Demo_ReadLocalCamera(void); // pkt_democam

boolean Demo_BeginPlayback(char *filename);
boolean Demo_ReadPacket(void);
void Demo_StopPlayback(void);

// Console commands.
D_CMD( PlayDemo );
D_CMD( RecordDemo );
D_CMD( PauseDemo );
D_CMD( StopDemo );
D_CMD( DemoLump );

#endif
