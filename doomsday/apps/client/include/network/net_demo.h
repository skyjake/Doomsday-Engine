/** @file
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

/**
 * Demos.
 */

#ifndef DE_DEMO_H
#define DE_DEMO_H

#ifdef __SERVER__
#  error Demos are not available in a SERVER build
#endif

// The consolePlayer's camera position is written to the demo file
// every 3rd tic.
#define LOCALCAM_WRITE_TICS 3

#ifdef __cplusplus
extern "C" {
#endif

extern int      playback;

void            Demo_Register(void);

void            Demo_Init(void);
void            Demo_Ticker(timespan_t time);

dd_bool         Demo_BeginRecording(const char* filename, int playernum);
void            Demo_StopRecording(int playernum);
void            Demo_PauseRecording(int playernum);
void            Demo_ResumeRecording(int playernum);
void            Demo_WritePacket(int playernum);
void            Demo_BroadcastPacket(void);
void            Demo_ReadLocalCamera(void); // PKT_DEMOCAM

dd_bool         Demo_BeginPlayback(const char* filename);
dd_bool         Demo_ReadPacket(void);
void            Demo_StopPlayback(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* DE_DEMO_H */
