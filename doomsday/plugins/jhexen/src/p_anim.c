/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * p_anim.c: Flat and Texture animations, parsed from ANIMDEFS.
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

#include "p_mapsetup.h"
#include "p_mapspec.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static void parseAnimGroup(gltexture_type_t type)
{
    boolean ignore = true, done;
    int groupNumber = 0;
    uint texNumBase = 0;

    if(!(type == GLT_FLAT || type == GLT_DOOMTEXTURE))
        Con_Error("parseAnimGroup: Internal Error, invalid type %i.", (int) type);

    if(!SC_GetString()) // Name.
    {
        SC_ScriptError("Missing string.");
    }

    if((texNumBase = GL_CheckTextureNumForName(sc_String, type)) != 0)
        ignore = false;

    if(!ignore)
        groupNumber = Materials_CreateAnimGroup(AGF_SMOOTH | AGF_FIRST_ONLY);

    done = false;
    do
    {
        if(SC_GetString())
        {
            if(SC_Compare("pic"))
            {
                int picNum, min = 0, max = 0;

                SC_MustGetNumber();
                picNum = sc_Number;

                SC_MustGetString();
                if(SC_Compare("tics"))
                {
                    SC_MustGetNumber();
                    min = sc_Number;
                }
                else if(SC_Compare("rand"))
                {
                    SC_MustGetNumber();
                    min = sc_Number;
                    SC_MustGetNumber();
                    max = sc_Number;
                }
                else
                {
                    SC_ScriptError(0);
                }

                if(!ignore)
                {
                    materialnum_t frame = DD_MaterialForTexture(texNumBase + picNum - 1, type);
                    if(frame != 0)
                        Materials_AddAnimGroupFrame(groupNumber, frame, min, (max > 0? max - min : 0));
                }
            }
            else
            {
                SC_UnGet();
                done = true;
            }
        }
        else
        {
            done = true;
        }
    } while(!done);
}

/**
 * Parse an ANIMDEFS definition for flat/texture animations.
 */
void P_InitPicAnims(void)
{
    lumpnum_t lump = W_CheckNumForName("ANIMDEFS");

    if(lump != -1)
    {
        SC_OpenLump(lump);

        while(SC_GetString())
        {
            if(SC_Compare("flat"))
            {
                parseAnimGroup(GLT_FLAT);
            }
            else if(SC_Compare("texture"))
            {
                parseAnimGroup(GLT_DOOMTEXTURE);
            }
            else
            {
                SC_ScriptError(0);
            }
        }

        SC_Close();
    }
}
