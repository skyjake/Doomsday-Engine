/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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

#define ANIM_SCRIPT_NAME        "ANIMDEFS"
#define MAX_ANIM_DEFS           20
#define MAX_FRAME_DEFS          96
#define SCI_FLAT                "flat"
#define SCI_TEXTURE             "texture"
#define SCI_PIC                 "pic"
#define SCI_TICS                "tics"
#define SCI_RAND                "rand"

// TYPES -------------------------------------------------------------------

typedef struct framedef_s {
    int     index;
    int     tics;
} framedef_t;

typedef struct animdef_s {
    int     type;
    int     index;
    int     tics;
    int     currentFrameDef;
    int     startFrameDef;
    int     endFrameDef;
} animdef_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Parse an ANIMDEFS definition for flat/texture animations.
 */
void P_InitPicAnims(void)
{
    int                 base;
    boolean             ignore;
    boolean             done;
    int                 groupNumber = 0;
    materialnum_t       numBase = 0, num = 0;
    materialtype_t      type = MAT_FLAT;

    SC_Open(ANIM_SCRIPT_NAME);
    while(SC_GetString())
    {
        if(SC_Compare(SCI_FLAT))
        {
            type = MAT_FLAT;
        }
        else if(SC_Compare(SCI_TEXTURE))
        {
            type = MAT_TEXTURE;
        }
        else
        {
            SC_ScriptError(NULL);
        }
        SC_MustGetString(); // Name

        ignore = false;
        if(!R_MaterialCheckNumForName(sc_String, type))
        {
            ignore = true;
        }
        else
        {
            numBase = R_MaterialNumForName(sc_String, type);
            groupNumber = R_CreateAnimGroup(AGF_SMOOTH | AGF_FIRST_ONLY);
        }

        done = false;
        while(done == false)
        {
            if(SC_GetString())
            {
                if(SC_Compare(SCI_PIC))
                {
                    SC_MustGetNumber();
                    if(ignore == false)
                    {
                        num = numBase + sc_Number - 1;
                    }

                    SC_MustGetString();
                    if(SC_Compare(SCI_TICS))
                    {
                        SC_MustGetNumber();
                        if(ignore == false)
                        {
                            R_AddToAnimGroup(groupNumber, num, sc_Number, 0);
                        }
                    }
                    else if(SC_Compare(SCI_RAND))
                    {
                        SC_MustGetNumber();
                        base = sc_Number;
                        SC_MustGetNumber();
                        if(ignore == false)
                        {
                            R_AddToAnimGroup(groupNumber, num, base, sc_Number - base);
                        }
                    }
                    else
                    {
                        SC_ScriptError(NULL);
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
        }
    }

    SC_Close();
}
