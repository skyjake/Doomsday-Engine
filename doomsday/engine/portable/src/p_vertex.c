/**\file p_vertex.c
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

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_play.h"

int Vertex_SetProperty(Vertex *vtx, const setargs_t *args)
{
    // Vertices are not writable through DMU.
    Con_Error("Vertex_SetProperty: Is not writable.\n");

    return false; // Continue iteration.
}

int Vertex_GetProperty(const Vertex* vtx, setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_X:
        DMU_GetValue(DMT_VERTEX_POS, &vtx->V_pos[VX], args, 0);
        break;
    case DMU_Y:
        DMU_GetValue(DMT_VERTEX_POS, &vtx->V_pos[VY], args, 0);
        break;
    case DMU_XY:
        DMU_GetValue(DMT_VERTEX_POS, &vtx->V_pos[VX], args, 0);
        DMU_GetValue(DMT_VERTEX_POS, &vtx->V_pos[VY], args, 1);
        break;
    default:
        Con_Error("Vertex_GetProperty: Has no property %s.\n", DMU_Str(args->prop));
    }

    return false; // Continue iteration.
}
