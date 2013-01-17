/** @file con_bar.cpp Console progress bar
 * @ingroup console
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h"
#include "de_system.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_ui.h"

// Time for the progress to reach the new target (seconds).
#define PROGRESS_DELTA_TIME     0.5f

typedef struct tval_s {
    int         value;
    timespan_t  time;
} tval_t;

static mutex_t progressMutex;

static int   progressMax;
static float progressStart, progressEnd;
static tval_t target, last;

static void lockProgress(boolean lock)
{
    if(lock)
    {
        Sys_Lock(progressMutex);
    }
    else
    {
        Sys_Unlock(progressMutex);
    }
}

void Con_InitProgress2(int maxProgress, float start, float end)
{
    progressStart = start;
    progressEnd = end;
    memset(&target, 0, sizeof(target));
    memset(&last, 0, sizeof(last));

    progressMax = maxProgress;
    if(!progressMutex)
        progressMutex = Sys_CreateMutex("ConBarProgressMutex");
}

void Con_InitProgress(int maxProgress)
{
    Con_InitProgress2(maxProgress, 0, 1);
}

void Con_ShutdownProgress(void)
{
    if(progressMutex)
    {
        Sys_DestroyMutex(progressMutex);
    }
}

static int currentProgress(void)
{
    timespan_t nowTime = Timer_RealSeconds();
    timespan_t span = target.time - last.time;

    if(nowTime >= target.time || span <= 0)
    {
        // Done.
        return target.value;
    }
    else
    {
        // Interpolate.
        return last.value + (target.value - last.value) * (nowTime - last.time) / span;
    }
}

boolean Con_IsProgressAnimationCompleted(void)
{
    boolean done;

    lockProgress(true);
    done = (Timer_RealSeconds() >= target.time);
    lockProgress(false);

    return done;
}

void Con_SetProgress(int progress)
{
    timespan_t nowTime;

    lockProgress(true);

    // Continue animation from the current value.
    nowTime = Timer_RealSeconds();
    last.value = currentProgress();
    last.time = nowTime;

    target.value = progress;
    target.time = nowTime + (progress < progressMax? PROGRESS_DELTA_TIME : PROGRESS_DELTA_TIME/2);

    lockProgress(false);
}

float Con_GetProgress(void)
{
    float prog;

    if(!progressMax) return 1.0f;

    lockProgress(true);
    prog = currentProgress();
    lockProgress(false);

    prog /= (float) progressMax;

    // Scale to the progress range.
    return progressStart + prog * (progressEnd - progressStart);
}
