/**
 * @file dd_world.h
 * World data.
 *
 * World data comprises the map and all the objects in it. The public API
 * includes accessing and modifying map data objects via DMU.
 *
 * @ingroup map
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef __DOOMSDAY_WORLD_H__
#define __DOOMSDAY_WORLD_H__

#include "dd_share.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup map
///@{

/**
 * Determines the type of the map data object.
 *
 * @param ptr  Pointer to a map data object.
 */
int             DMU_GetType(const void* ptr);

unsigned int    P_ToIndex(const void* ptr);
void*           P_ToPtr(int type, uint index);
int             P_Callback(int type, uint index, void* context,
                           int (*callback)(void* p, void* ctx));
int             P_Callbackp(int type, void* ptr, void* context,
                            int (*callback)(void* p, void* ctx));
int             P_Iteratep(void *ptr, uint prop, void* context,
                           int (*callback) (void* p, void* ctx));

/* dummy objects */
void*           P_AllocDummy(int type, void* extraData);
void            P_FreeDummy(void* dummy);
boolean         P_IsDummy(void* dummy);
void*           P_DummyExtraData(void* dummy);

uint            P_CountGameMapObjs(int entityId);
byte            P_GetGMOByte(int entityId, uint elementIndex, int propertyId);
short           P_GetGMOShort(int entityId, uint elementIndex, int propertyId);
int             P_GetGMOInt(int entityId, uint elementIndex, int propertyId);
fixed_t         P_GetGMOFixed(int entityId, uint elementIndex, int propertyId);
angle_t         P_GetGMOAngle(int entityId, uint elementIndex, int propertyId);
float           P_GetGMOFloat(int entityId, uint elementIndex, int propertyId);

///@}

/// @addtogroup dmu
///@{

/* index-based write functions */
void            P_SetBool(int type, uint index, uint prop, boolean param);
void            P_SetByte(int type, uint index, uint prop, byte param);
void            P_SetInt(int type, uint index, uint prop, int param);
void            P_SetFixed(int type, uint index, uint prop, fixed_t param);
void            P_SetAngle(int type, uint index, uint prop, angle_t param);
void            P_SetFloat(int type, uint index, uint prop, float param);
void            P_SetDouble(int type, uint index, uint prop, double param);
void            P_SetPtr(int type, uint index, uint prop, void* param);

void            P_SetBoolv(int type, uint index, uint prop, boolean* params);
void            P_SetBytev(int type, uint index, uint prop, byte* params);
void            P_SetIntv(int type, uint index, uint prop, int* params);
void            P_SetFixedv(int type, uint index, uint prop, fixed_t* params);
void            P_SetAnglev(int type, uint index, uint prop, angle_t* params);
void            P_SetFloatv(int type, uint index, uint prop, float* params);
void            P_SetDoublev(int type, uint index, uint prop, double* params);
void            P_SetPtrv(int type, uint index, uint prop, void* params);

/* pointer-based write functions */
void            P_SetBoolp(void* ptr, uint prop, boolean param);
void            P_SetBytep(void* ptr, uint prop, byte param);
void            P_SetIntp(void* ptr, uint prop, int param);
void            P_SetFixedp(void* ptr, uint prop, fixed_t param);
void            P_SetAnglep(void* ptr, uint prop, angle_t param);
void            P_SetFloatp(void* ptr, uint prop, float param);
void            P_SetDoublep(void* ptr, uint prop, double param);
void            P_SetPtrp(void* ptr, uint prop, void* param);

void            P_SetBoolpv(void* ptr, uint prop, boolean* params);
void            P_SetBytepv(void* ptr, uint prop, byte* params);
void            P_SetIntpv(void* ptr, uint prop, int* params);
void            P_SetFixedpv(void* ptr, uint prop, fixed_t* params);
void            P_SetAnglepv(void* ptr, uint prop, angle_t* params);
void            P_SetFloatpv(void* ptr, uint prop, float* params);
void            P_SetDoublepv(void* ptr, uint prop, double* params);
void            P_SetPtrpv(void* ptr, uint prop, void* params);

/* index-based read functions */
boolean         P_GetBool(int type, uint index, uint prop);
byte            P_GetByte(int type, uint index, uint prop);
int             P_GetInt(int type, uint index, uint prop);
fixed_t         P_GetFixed(int type, uint index, uint prop);
angle_t         P_GetAngle(int type, uint index, uint prop);
float           P_GetFloat(int type, uint index, uint prop);
double          P_GetDouble(int type, uint index, uint prop);
void*           P_GetPtr(int type, uint index, uint prop);

void            P_GetBoolv(int type, uint index, uint prop, boolean* params);
void            P_GetBytev(int type, uint index, uint prop, byte* params);
void            P_GetIntv(int type, uint index, uint prop, int* params);
void            P_GetFixedv(int type, uint index, uint prop, fixed_t* params);
void            P_GetAnglev(int type, uint index, uint prop, angle_t* params);
void            P_GetFloatv(int type, uint index, uint prop, float* params);
void            P_GetDoublev(int type, uint index, uint prop, double* params);
void            P_GetPtrv(int type, uint index, uint prop, void* params);

/* pointer-based read functions */
boolean         P_GetBoolp(void* ptr, uint prop);
byte            P_GetBytep(void* ptr, uint prop);
int             P_GetIntp(void* ptr, uint prop);
fixed_t         P_GetFixedp(void* ptr, uint prop);
angle_t         P_GetAnglep(void* ptr, uint prop);
float           P_GetFloatp(void* ptr, uint prop);
double          P_GetDoublep(void* ptr, uint prop);
void*           P_GetPtrp(void* ptr, uint prop);

void            P_GetBoolpv(void* ptr, uint prop, boolean* params);
void            P_GetBytepv(void* ptr, uint prop, byte* params);
void            P_GetIntpv(void* ptr, uint prop, int* params);
void            P_GetFixedpv(void* ptr, uint prop, fixed_t* params);
void            P_GetAnglepv(void* ptr, uint prop, angle_t* params);
void            P_GetFloatpv(void* ptr, uint prop, float* params);
void            P_GetDoublepv(void* ptr, uint prop, double* params);
void            P_GetPtrpv(void* ptr, uint prop, void* params);

///@}

#ifdef __cplusplus
} // extern "C"
#endif

///@}

#endif // __DOOMSDAY_WORLD_H__
