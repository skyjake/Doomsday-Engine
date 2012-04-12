/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * p_dmu.h: Map Update API
 *
 * Engine-internal header for DMU.
 */

#ifndef __DOOMSDAY_MAP_UPDATE_H__
#define __DOOMSDAY_MAP_UPDATE_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct setargs_s {
    int             type;
    uint            prop;
    int             modifiers; // Property modifiers (e.g., line of sector)
    valuetype_t     valueType;
    boolean        *booleanValues;
    byte           *byteValues;
    int            *intValues;
    fixed_t        *fixedValues;
    float          *floatValues;
    double         *doubleValues;
    angle_t        *angleValues;
    void          **ptrValues;
} setargs_t;

void        P_InitMapUpdate(void);
void       *P_AllocDummy(int type, void* extraData);
void        P_FreeDummy(void* dummy);
int         P_DummyType(void* dummy);
boolean     P_IsDummy(void* dummy);
void       *P_DummyExtraData(void* dummy);

uint        P_ToIndex(const void* ptr);

const char *DMU_Str(uint prop);
int         DMU_GetType(const void* ptr);
void        DMU_SetValue(valuetype_t valueType, void* dst,
                         const setargs_t* args, uint index);
void        DMU_GetValue(valuetype_t valueType, const void* src,
                         setargs_t* args, uint index);

/// To be called to update in response to a DMU property change event.
int DMU_SetMaterialProperty(struct material_s* material, const setargs_t* args);

/// Get the current value of a Material property.
int DMU_GetMaterialProperty(struct material_s* material, setargs_t* args);

#ifndef NDEBUG
# define ASSERT_DMU_TYPE(ptr, dmuType) \
    if(!ptr || ((runtime_mapdata_header_t*)ptr)->type != dmuType) \
        Con_Error("ASSERT_DMU_TYPE failure on line %i in "__FILE__". " #ptr " is not %s.\n", __LINE__, DMU_Str(dmuType));
#else
# define ASSERT_DMU_TYPE(ptr, dmuType)
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif
