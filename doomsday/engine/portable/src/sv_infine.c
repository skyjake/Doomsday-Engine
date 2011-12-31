/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
 * Server-side InFine.
 */

#include "de_base.h"
#include "de_network.h"
#include "de_infine.h"

/**
 * The actual script is sent to the clients. 'script' can be NULL.
 */
void Sv_Finale(int flags, const char* script)
{
    size_t scriptLen = 0, len = 1 /* Just enough memory for the flags byte */;
    byte* buffer, *ptr;

    assert(isServer);

    // How much memory do we need?
    if(flags & FINF_SCRIPT)
    {
        scriptLen = strlen(script) + 1;
        len += scriptLen + 1; // The end null.
    }

    ptr = buffer = malloc(len);

    // First the flags.
    *ptr++ = flags;
    if(script)
    {
        memcpy(ptr, script, scriptLen + 1);
        ptr[scriptLen] = '\0';
    }

    Net_SendPacket(DDSP_ALL_PLAYERS | DDSP_ORDERED, PSV_FINALE2, buffer, len);

    free(buffer);
}
