/**\file p_anim.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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

#include "jhexen.h"

#include "p_mapsetup.h"
#include "p_mapspec.h"

static void parseAnimGroup(texturenamespaceid_t texNamespace)
{
    boolean ignore = true, done;
    int groupNumber = 0;
    uint texNumBase = 0;

    if(!(texNamespace == TN_FLATS || texNamespace == TN_TEXTURES))
        Con_Error("parseAnimGroup: Internal Error, invalid namespace %i.",
                  (int) texNamespace);

    if(!SC_GetString()) // Name.
    {
        SC_ScriptError("Missing string.");
    }

    if((texNumBase = GL_TextureIndexForName(sc_String, texNamespace)) != 0)
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
                    materialnum_t frame = DD_MaterialForTextureIndex(texNumBase + picNum - 1, texNamespace);
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
                parseAnimGroup(TN_FLATS);
            }
            else if(SC_Compare("texture"))
            {
                parseAnimGroup(TN_TEXTURES);
            }
            else
            {
                SC_ScriptError(0);
            }
        }

        SC_Close();
    }
}
