/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
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
