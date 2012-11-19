/**\file p_anim.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

static void parseAnimGroup(boolean isTexture, boolean isCustom)
{
    boolean ignore = true, done;
    int groupNumber = 0;
    int texNumBase = -1;
    ddstring_t path;
    Uri* uri;

    if(!SC_GetString()) // Name.
    {
        SC_ScriptError("Missing string.");
    }

    uri = Uri_New();
    Uri_SetScheme(uri, isTexture? TN_TEXTURES_NAME : TN_FLATS_NAME);
    Str_Init(&path);
    Str_PercentEncode(Str_Set(&path, sc_String));
    Uri_SetPath(uri, Str_Text(&path));
    Str_Free(&path);

    texNumBase = R_TextureUniqueId2(uri, !isCustom);
    if(texNumBase != -1)
        ignore = false;

    if(!ignore)
        groupNumber = R_CreateAnimGroup(AGF_SMOOTH | AGF_FIRST_ONLY);

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
                    Uri_Delete(uri);
                    SC_ScriptError(0);
                }

                if(!ignore)
                {
                    Uri* frameUrn = Uri_NewWithPath2("urn:", FC_NONE);
                    ddstring_t framePath;

                    Str_Init(&framePath);
                    Str_Appendf(&framePath, "%s:%i", isTexture? TN_TEXTURES_NAME : TN_FLATS_NAME, texNumBase + picNum - 1);
                    Uri_SetPath(frameUrn, Str_Text(&framePath));

                    R_AddAnimGroupFrame(groupNumber, frameUrn, min, (max > 0? max - min : 0));

                    Str_Free(&framePath);
                    Uri_Delete(frameUrn);
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

    Uri_Delete(uri);
}

void P_InitPicAnims(void)
{
    lumpnum_t lumpNum = W_CheckLumpNumForName("ANIMDEFS");
    boolean isCustom, isTexture = false; // Shut up compiler!

    if(lumpNum == -1) return;
    isCustom = W_LumpIsCustom(lumpNum);

    SC_OpenLump(lumpNum);
    while(SC_GetString())
    {
        if(SC_Compare("texture"))
        {
            isTexture = true;
        }
        else if(SC_Compare("flat"))
        {
            isTexture = false;
        }
        else
        {
            SC_ScriptError(0);
        }

        parseAnimGroup(isTexture, isCustom);
    }
    SC_Close();
}
