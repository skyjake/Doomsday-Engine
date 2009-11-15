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
 * p_plat.h: Common playsim routines relating to moving platforms.
 */

#ifndef __COMMON_THINKER_PLAT_H__
#define __COMMON_THINKER_PLAT_H__

#if __JDOOM__
#   include "d_identifiers.h"
#endif

#include <de/Thinker>

typedef enum {
    PS_UP, // Moving up.
    PS_DOWN, // Moving down.
    PS_WAIT
} platstate_e;

typedef enum {
    PT_PERPETUALRAISE,
    PT_DOWNWAITUPSTAY,
#if __JHEXEN__
    PT_DOWNBYVALUEWAITUPSTAY,
    PT_UPWAITDOWNSTAY,
    PT_UPBYVALUEWAITDOWNSTAY,
#else
# if __JDOOM64__
    PT_UPWAITDOWNSTAY, //jd64 kaiser - outcast
    PT_DOWNWAITUPDOOR, //jd64 kaiser - outcast
# endif
    PT_RAISEANDCHANGE,
    PT_RAISETONEARESTANDCHANGE,
# if __JDOOM__ || __JDOOM64__
    PT_DOWNWAITUPSTAYBLAZE,
#  if __JDOOM64__
    PT_DOWNWAITUPPLUS16STAYBLAZE, //jd64
#  endif
# endif
#endif
    NUMPLATTYPES
} plattype_e;

class PlatThinker : public de::Thinker
{
public:
    sector_t*       sector;
    float           speed;
    float           low;
    float           high;
    int             wait;
    int             count;
    platstate_e     state;
    platstate_e     oldState;
    boolean         crush;
    int             tag;
    plattype_e      type;
    
public:
    PlatThinker()
        : de::Thinker(SID_PLAT_THINKER),
          sector(0),
          speed(0),
          low(0),
          high(0),
          wait(0),
          count(0),
          state(PS_UP),
          oldState(PS_UP),
          crush(false),
          tag(0),
          type(PT_PERPETUALRAISE) {}
          
    void think(const de::Time::Delta& elapsed);

    // Implements ISerializable.
    void operator >> (de::Writer& to) const;
    void operator << (de::Reader& from);

    static Thinker* construct() {
        return new PlatThinker;
    }
};

typedef PlatThinker plat_t;

#define PLATWAIT        (3)
#define PLATSPEED       (1)

//void        T_PlatRaise(plat_t* pl);

#if __JHEXEN__
int         EV_DoPlat(linedef_t* li, byte* args, plattype_e type,
                      int amount);
int         P_PlatDeactivate(short tag);
#else
int         EV_DoPlat(linedef_t* li, plattype_e type, int amount);
int         P_PlatActivate(short tag);
int         P_PlatDeactivate(short tag);
#endif

#endif
