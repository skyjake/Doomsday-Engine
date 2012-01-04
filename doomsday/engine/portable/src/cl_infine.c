/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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
 * Client-side InFine.
 */
#include "de_base.h"
#include "de_network.h"
#include "de_infine.h"

static finaleid_t currentFinale = 0;

/**
 * This is where clients start their InFine sequences.
 */
void Cl_Finale(Reader* msg)
{
    int flags = Reader_ReadByte(msg);
    byte* script = 0;
    int len;

    if(flags & FINF_SCRIPT)
    {
        // Read the script into map-scope memory. It will be freed
        // when the next map is loaded.
        len = Reader_ReadUInt32(msg);
        script = malloc(len + 1);
        Reader_Read(msg, script, len);
        script[len] = 0;
    }

    if((flags & FINF_SCRIPT) && (flags & FINF_BEGIN))
    {
        // Start the script.
        currentFinale = FI_Execute(script, FF_LOCAL);
#ifdef _DEBUG
        Con_Message("Cl_Finale: Started finale %i.\n", currentFinale);
#endif
    }

    if((flags & FINF_END) && currentFinale)
    {
        FI_ScriptTerminate(currentFinale);
        currentFinale = 0;
    }

    if((flags & FINF_SKIP) && currentFinale)
    {       
        FI_ScriptRequestSkip(currentFinale);
    }

    if(script) free(script);
}
