/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "driver_fmod.h"

int DM_CDAudio_Init(void)
{
    return fmodSystem != 0;
}

void DM_CDAudio_Shutdown(void)
{
    // Will be shut down with the rest of FMOD.
}

void DM_CDAudio_Update(void)
{}

void DM_CDAudio_Set(int prop, float value)
{
    if(!fmodSystem) return;
}

int DM_CDAudio_Get(int prop, void* value)
{
    if(!fmodSystem) return 0;
    return 0;
}

int DM_CDAudio_Play(int track, int looped)
{
    if(!fmodSystem) return 0;
    return 0;
}

void DM_CDAudio_Pause(int pause)
{
    if(!fmodSystem) return;

}

void DM_CDAudio_Stop(void)
{
    if(!fmodSystem) return;

}
