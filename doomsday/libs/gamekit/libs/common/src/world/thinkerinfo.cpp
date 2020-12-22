/** @file thinkerinfo.cpp  Game save thinker info.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "common.h"
#include "thinkerinfo.h"

#include "acs/interpreter.h"
#include "acs/script.h"
#include "mobj.h"
#include "p_ceiling.h"
#include "p_door.h"
#include "p_floor.h"
#include "p_plat.h"
#include "p_scroll.h"
#include "p_switch.h"
#if __JHEXEN__
#  include "p_pillar.h"
#  include "p_waggle.h"
#endif
#include "polyobjs.h"

template <typename Type>
static void writeThinkerAs(const thinker_t *th, MapStateWriter *msWriter)
{
    Type *t = (Type*)th;
    t->write(msWriter);
}

template <typename Type>
static int readThinkerAs(thinker_t *th, MapStateReader *msReader)
{
    Type *t = (Type *)th;
    return t->read(msReader);
}

static ThinkerClassInfo thinkerInfo[] = {
    {
      TC_MOBJ,
      (thinkfunc_t) P_MobjThinker,
      TSF_SERVERONLY,
      de::function_cast<WriteThinkerFunc>(writeThinkerAs<mobj_s>),
      de::function_cast<ReadThinkerFunc>(readThinkerAs<mobj_s>),
      sizeof(mobj_t)
    },
#if !__JHEXEN__
    {
      TC_XGMOVER,
      (thinkfunc_t) XS_PlaneMover,
      0,
      de::function_cast<WriteThinkerFunc>(writeThinkerAs<xgplanemover_s>),
      de::function_cast<ReadThinkerFunc>(readThinkerAs<xgplanemover_s>),
      sizeof(xgplanemover_t)
    },
#endif
    {
      TC_CEILING,
      T_MoveCeiling,
      0,
      de::function_cast<WriteThinkerFunc>(writeThinkerAs<ceiling_t>),
      de::function_cast<ReadThinkerFunc>(readThinkerAs<ceiling_t>),
      sizeof(ceiling_t)
    },
    {
      TC_DOOR,
      T_Door,
      0,
      de::function_cast<WriteThinkerFunc>(writeThinkerAs<door_t>),
      de::function_cast<ReadThinkerFunc>(readThinkerAs<door_t>),
      sizeof(door_t)
    },
    {
      TC_FLOOR,
      T_MoveFloor,
      0,
      de::function_cast<WriteThinkerFunc>(writeThinkerAs<floor_t>),
      de::function_cast<ReadThinkerFunc>(readThinkerAs<floor_t>),
      sizeof(floor_t)
    },
    {
      TC_PLAT,
      T_PlatRaise,
      0,
      de::function_cast<WriteThinkerFunc>(writeThinkerAs<plat_t>),
      de::function_cast<ReadThinkerFunc>(readThinkerAs<plat_t>),
      sizeof(plat_t)
    },
#if __JHEXEN__
    {
     TC_INTERPRET_ACS,
     (thinkfunc_t) acs_Interpreter_Think,
     0,
     de::function_cast<WriteThinkerFunc>(writeThinkerAs<acs::Interpreter>),
     de::function_cast<ReadThinkerFunc>(readThinkerAs<acs::Interpreter>),
     sizeof(acs::Interpreter)
    },
    {
     TC_FLOOR_WAGGLE,
     (thinkfunc_t) T_FloorWaggle,
     0,
     de::function_cast<WriteThinkerFunc>(writeThinkerAs<waggle_t>),
     de::function_cast<ReadThinkerFunc>(readThinkerAs<waggle_t>),
     sizeof(waggle_t)
    },
    {
     TC_LIGHT,
     (thinkfunc_t) T_Light,
     0,
     de::function_cast<WriteThinkerFunc>(writeThinkerAs<light_t>),
     de::function_cast<ReadThinkerFunc>(readThinkerAs<light_t>),
     sizeof(light_t)
    },
    {
     TC_PHASE,
     (thinkfunc_t) T_Phase,
     0,
     de::function_cast<WriteThinkerFunc>(writeThinkerAs<phase_t>),
     de::function_cast<ReadThinkerFunc>(readThinkerAs<phase_t>),
     sizeof(phase_t)
    },
    {
     TC_BUILD_PILLAR,
     (thinkfunc_t) T_BuildPillar,
     0,
     de::function_cast<WriteThinkerFunc>(writeThinkerAs<pillar_t>),
     de::function_cast<ReadThinkerFunc>(readThinkerAs<pillar_t>),
     sizeof(pillar_t)
    },
    {
     TC_ROTATE_POLY,
     T_RotatePoly,
     0,
     de::function_cast<WriteThinkerFunc>(writeThinkerAs<polyevent_t>),
     de::function_cast<ReadThinkerFunc>(readThinkerAs<polyevent_t>),
     sizeof(polyevent_t)
    },
    {
     TC_MOVE_POLY,
     T_MovePoly,
     0,
     de::function_cast<WriteThinkerFunc>(SV_WriteMovePoly),
     de::function_cast<ReadThinkerFunc>(SV_ReadMovePoly),
     sizeof(polyevent_t)
    },
    {
     TC_POLY_DOOR,
     T_PolyDoor,
     0,
     de::function_cast<WriteThinkerFunc>(writeThinkerAs<polydoor_t>),
     de::function_cast<ReadThinkerFunc>(readThinkerAs<polydoor_t>),
     sizeof(polydoor_t)
    },
#else
    {
      TC_FLASH,
      (thinkfunc_t) T_LightFlash,
      0,
      de::function_cast<WriteThinkerFunc>(writeThinkerAs<lightflash_s>),
      de::function_cast<ReadThinkerFunc>(readThinkerAs<lightflash_s>),
      sizeof(lightflash_s)
    },
    {
      TC_STROBE,
      (thinkfunc_t) T_StrobeFlash,
      0,
      de::function_cast<WriteThinkerFunc>(writeThinkerAs<strobe_t>),
      de::function_cast<ReadThinkerFunc>(readThinkerAs<strobe_t>),
      sizeof(strobe_t)
    },
    {
      TC_GLOW,
      (thinkfunc_t) T_Glow,
      0,
      de::function_cast<WriteThinkerFunc>(writeThinkerAs<glow_t>),
      de::function_cast<ReadThinkerFunc>(readThinkerAs<glow_t>),
      sizeof(glow_t)
    },
# if __JDOOM__ || __JDOOM64__
    {
      TC_FLICKER,
      (thinkfunc_t) T_FireFlicker,
      0,
      de::function_cast<WriteThinkerFunc>(writeThinkerAs<fireflicker_t>),
      de::function_cast<ReadThinkerFunc>(readThinkerAs<fireflicker_t>),
      sizeof(fireflicker_t)
    },
# endif
# if __JDOOM64__
    {
      TC_BLINK,
      (thinkfunc_t) T_LightBlink,
      0,
      de::function_cast<WriteThinkerFunc>(writeThinkerAs<lightblink_t>),
      de::function_cast<ReadThinkerFunc>(readThinkerAs<lightblink_t>),
      sizeof(lightblink_t)
    },
# endif
#endif
    {
      TC_MATERIALCHANGER,
      T_MaterialChanger,
      0,
      de::function_cast<WriteThinkerFunc>(writeThinkerAs<materialchanger_s>),
      de::function_cast<ReadThinkerFunc>(readThinkerAs<materialchanger_s>),
      sizeof(materialchanger_s)
    },
    {
      TC_SCROLL,
      (thinkfunc_t) T_Scroll,
      0,
      de::function_cast<WriteThinkerFunc>(writeThinkerAs<scroll_t>),
      de::function_cast<ReadThinkerFunc>(readThinkerAs<scroll_t>),
      sizeof(scroll_t)
    },
    { TC_NULL, NULL, 0, NULL, NULL, 0 }
};

ThinkerClassInfo *SV_ThinkerInfoForClass(thinkerclass_t tClass)
{
    for(ThinkerClassInfo *info = thinkerInfo; info->thinkclass != TC_NULL; info++)
    {
        if(info->thinkclass == tClass)
            return info;
    }
    return 0; // Not found.
}

ThinkerClassInfo *SV_ThinkerInfo(const thinker_t &thinker)
{
    for(ThinkerClassInfo *info = thinkerInfo; info->thinkclass != TC_NULL; info++)
    {
        if(info->function == thinker.function)
            return info;
    }
    return 0; // Not found.
}
