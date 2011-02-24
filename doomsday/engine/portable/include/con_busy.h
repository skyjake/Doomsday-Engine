/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
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
 * con_busy.h: Console Busy Mode
 */

#ifndef __DOOMSDAY_CONSOLE_BUSY_H__
#define __DOOMSDAY_CONSOLE_BUSY_H__

typedef int     (C_DECL *busyworkerfunc_t) (void* parm);

typedef enum {
    FIRST_TRANSITIONSTYLE,
    TS_CROSSFADE = FIRST_TRANSITIONSTYLE, // Basic opacity crossfade.
    TS_DOOMSMOOTH, // Emulates the DOOM "blood on wall" screen wipe (smoothed).
    TS_DOOM, // Emulates the DOOM "blood on wall" screen wipe.
    LAST_TRANSITIONSTYLE = TS_DOOM
} transitionstyle_t;

extern int rTransition;
extern int rTransitionTics;

int         Con_Busy(int flags, const char* taskName,
                     busyworkerfunc_t worker, void *workerData);
boolean     Con_IsBusy(void);
void        Con_BusyWorkerEnd(void);
void        Con_BusyWorkerError(const char* message);
void        Con_AcquireScreenshotTexture(void);
void        Con_ReleaseScreenshotTexture(void);

boolean     Con_TransitionInProgress(void);
void        Con_TransitionTicker(timespan_t ticLength);
void        Con_DrawTransition(void);

#endif
