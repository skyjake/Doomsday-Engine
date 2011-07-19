/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2011 Daniel Swanson <danij@dengine.net>
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
 * dd_loop.h: Main Loop
 */

#ifndef __DOOMSDAY_BASELOOP_H__
#define __DOOMSDAY_BASELOOP_H__

extern boolean appShutdown;
#ifdef WIN32
extern boolean suspendMsgPump;
#endif

extern int rFrameCount;
extern timespan_t sysTime, gameTime, demoTime, ddMapTime;
extern boolean tickFrame;

void            DD_RegisterLoop(void);
int             DD_GameLoop(void);
void            DD_DrawAndBlit(void);
void            DD_StartFrame(void);
void            DD_EndFrame(void);
float           DD_GetFrameRate(void);
void            DD_ResetTimer(void);
boolean         DD_IsSharpTick(void);

#endif
