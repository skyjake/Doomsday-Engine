/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * sys_mixer.c: Volume Control
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_audio.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int initMixerOk = 0;

// CODE --------------------------------------------------------------------

int Sys_InitMixer(void)
{
    if(initMixerOk || ArgCheck("-nomixer") || ArgCheck("-nomusic") || isDedicated)
        return true;

    // We're successful.
    initMixerOk = true;
    return true;
}

void Sys_ShutdownMixer(void)
{
    if(!initMixerOk)
        return; // Can't un-initialize if not inited.

    initMixerOk = false;
}

int Sys_Mixer4i(int device, int action, int control, int parm)
{
    if(!initMixerOk)
        return MIX_ERROR;

    // There is currently no implementation for anything.
    return MIX_ERROR;
}

int Sys_Mixer3i(int device, int action, int control)
{
    return Sys_Mixer4i(device, action, control, 0);
}
