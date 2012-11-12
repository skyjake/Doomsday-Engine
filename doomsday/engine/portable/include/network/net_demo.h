/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * Demos.
 */

#ifndef LIBDENG_DEMO_H
#define LIBDENG_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

extern int      playback;

void            Demo_Register(void);

void            Demo_Init(void);
void            Demo_Ticker(timespan_t time);

boolean         Demo_BeginRecording(const char* filename, int playernum);
void            Demo_StopRecording(int playernum);
void            Demo_PauseRecording(int playernum);
void            Demo_ResumeRecording(int playernum);
void            Demo_WritePacket(int playernum);
void            Demo_BroadcastPacket(void);
void            Demo_ReadLocalCamera(void); // PKT_DEMOCAM

boolean         Demo_BeginPlayback(const char* filename);
boolean         Demo_ReadPacket(void);
void            Demo_StopPlayback(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_DEMO_H */
