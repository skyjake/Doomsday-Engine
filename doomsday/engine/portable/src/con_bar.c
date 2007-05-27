/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
 * con_bar.c: Console Progress Bar
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_ui.h"

#include "sys_stwin.h"

// MACROS ------------------------------------------------------------------

// Time for the progress to reach the new target (seconds).
#define PROGRESS_DELTA_TIME 1.0

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

typedef struct tval_s {
    int value;
    timespan_t time;
} tval_t;

static int progressMax;
static tval_t target, last;
static mutex_t progressMutex;

// CODE --------------------------------------------------------------------

void Con_LockProgress(boolean lock)
{
    if(!progressMutex)
    {
        progressMutex = Sys_CreateMutex("ConBarProgressMutex");
    }
    
    if(lock)
    {
        Sys_Lock(progressMutex);
    }
    else
    {
        Sys_Unlock(progressMutex);
    }
}

void Con_InitProgress(int maxProgress)
{
    memset(&target, 0, sizeof(target));
    memset(&last, 0, sizeof(last));
    progressMax = maxProgress;
}

/*
 * Updates the progress.
 */
void Con_SetProgress(int progress)
{
    timespan_t nowTime;
    
    Con_LockProgress(true);
    
    nowTime = Sys_GetRealSeconds();

    if(nowTime >= target.time)
    {
        // Previous movement has ended.
        last.time = nowTime;
        last.value = target.value;
    }
    
    target.value = progress;
    target.time = Sys_GetRealSeconds();
    if(target.value < progressMax)
    {
        float delta = target.time - last.time;
        if(delta < PROGRESS_DELTA_TIME)
        {
            delta = (delta + PROGRESS_DELTA_TIME) / 2;
        }
        target.time += delta;
    }
    else
    {
        target.value = progressMax;
    }
    
    Con_LockProgress(false); 
}

/**
 * Calculate the progress at the current time.
 */
float Con_GetProgress(void)
{
    timespan_t nowTime, span;
    float retValue = 1.0;
    
    Con_LockProgress(true);    
    
    nowTime = Sys_GetRealSeconds();
    span = target.time - last.time;
        
    if(progressMax)
    {
        if(nowTime >= target.time)
        {        
            retValue = target.value/(float)progressMax; // Done.
        }
        // Interpolate.
        else if(span <= 0)
        {
            retValue = target.value;
        }
        else
        {
            retValue = (last.value + (target.value - last.value) * (nowTime - last.time) / span) 
                / progressMax;
        }
    }

    Con_LockProgress(false);    
    
    return retValue;
}
