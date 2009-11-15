/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * p_switch.h: Common playsim routines relating to switches.
 */

#ifndef __COMMON_SWITCH_H__
#define __COMMON_SWITCH_H__

#if __JDOOM__
#   include "d_identifiers.h"
#endif

#include <de/Thinker>

typedef enum {
    SID_MIDDLE,
    SID_BOTTOM,
    SID_TOP
} sidedefsurfaceid_t;

#define BUTTONTIME              (TICSPERSEC) // 1 second, in ticks.

class MaterialChangerThinker : public de::Thinker
{
public:
    int             timer;
    sidedef_t*      side;
    sidedefsurfaceid_t ssurfaceID;
    material_t*     material;
    
public:
    MaterialChangerThinker() 
        : de::Thinker(SID_MATERIAL_CHANGER_THINKER),
          timer(0),
          side(0),
          ssurfaceID(SID_MIDDLE),
          material(0) {}
    
    void think(const de::Time::Delta& elapsed);

    // Implements ISerializable.
    void operator >> (de::Writer& to) const;
    void operator << (de::Reader& from);

    static Thinker* construct() {
        return new MaterialChangerThinker;
    }    
};
    
typedef MaterialChangerThinker materialchanger_t;

/**
 * This struct is used to provide byte offsets when reading a custom
 * SWITCHES lump thus it must be packed and cannot be altered.
 */
#pragma pack(1)
typedef struct {
    /* Do NOT change these members in any way! */
    char            name1[9];
    char            name2[9];
#if __JHEXEN__
    int             soundID;
#else
    short           episode;
#endif
} switchlist_t;
#pragma pack()

void            P_InitSwitchList(void);
material_t*     P_GetSwitch(material_t* mat, const switchlist_t** info);

boolean         P_ToggleSwitch(sidedef_t* side, int sound, boolean silent,
                               int tics);
boolean         P_UseSpecialLine(mobj_t* mo, linedef_t* line, int side);

void            P_SpawnMaterialChanger(sidedef_t* side, sidedefsurfaceid_t ssurfaceID,
                                       material_t* mat, int tics);

#endif
