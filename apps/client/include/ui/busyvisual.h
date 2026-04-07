/**
 * @file busyvisual.h
 * Busy Mode visualizer. @ingroup render
 *
 * @authors Copyright © 2007-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DE_RENDER_BUSYVISUAL_H
#define DE_RENDER_BUSYVISUAL_H

#include "api_busy.h"

#ifdef __cplusplus
extern "C" {
#endif

void BusyVisual_PrepareResources(void);

/**
 * @todo Does the console transition animation really belong in the busy visual?
 */
/// Busy mode transition style.
typedef enum {
    FIRST_TRANSITIONSTYLE,
    TS_CROSSFADE = FIRST_TRANSITIONSTYLE, ///< Basic opacity crossfade.
    TS_DOOMSMOOTH, ///< Emulates the DOOM "blood on wall" screen wipe (smoothed).
    TS_DOOM, ///< Emulates the DOOM "blood on wall" screen wipe.
    LAST_TRANSITIONSTYLE = TS_DOOM
} transitionstyle_t;

#ifdef __CLIENT__

extern int rTransition;
extern int rTransitionTics;

void Con_TransitionRegister(void);

void Con_TransitionConfigure(void);
void Con_TransitionBegin(void);

/// @return  @c true if a busy mode transition animation is currently in progress.
dd_bool Con_TransitionInProgress(void);

void Con_TransitionTicker(timespan_t ticLength);
void Con_DrawTransition(void);

#endif // __CLIENT__

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* DE_RENDER_BUSYVISUAL_H */
