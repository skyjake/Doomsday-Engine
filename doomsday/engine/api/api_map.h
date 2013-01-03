/** @file api_map.h Map data.
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

#ifndef __DOOMSDAY_MAP_H__
#define __DOOMSDAY_MAP_H__

#include "dd_share.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup map
///@{

DENG_API_TYPEDEF(Map)
{
    de_api_t api;

    /**
     * Determines the type of the map data object.
     *
     * @param ptr  Pointer to a map data object.
     */
    int             (*GetType)(const void* ptr);

    unsigned int    (*ToIndex)(const void* ptr);
    void*           (*ToPtr)(int type, uint index);
    int             (*Callback)(int type, uint index, void* context, int (*callback)(void* p, void* ctx));
    int             (*Callbackp)(int type, void* ptr, void* context, int (*callback)(void* p, void* ctx));
    int             (*Iteratep)(void *ptr, uint prop, void* context, int (*callback) (void* p, void* ctx));

    /* dummy objects */
    void*           (*AllocDummy)(int type, void* extraData);
    void            (*FreeDummy)(void* dummy);
    boolean         (*IsDummy)(void* dummy);
    void*           (*DummyExtraData)(void* dummy);

    uint            (*CountGameMapObjs)(int entityId);
    byte            (*GetGMOByte)(int entityId, uint elementIndex, int propertyId);
    short           (*GetGMOShort)(int entityId, uint elementIndex, int propertyId);
    int             (*GetGMOInt)(int entityId, uint elementIndex, int propertyId);
    fixed_t         (*GetGMOFixed)(int entityId, uint elementIndex, int propertyId);
    angle_t         (*GetGMOAngle)(int entityId, uint elementIndex, int propertyId);
    float           (*GetGMOFloat)(int entityId, uint elementIndex, int propertyId);

    ///@}

    /// @addtogroup dmu
    ///@{

    /* index-based write functions */
    void            (*SetBool)(int type, uint index, uint prop, boolean param);
    void            (*SetByte)(int type, uint index, uint prop, byte param);
    void            (*SetInt)(int type, uint index, uint prop, int param);
    void            (*SetFixed)(int type, uint index, uint prop, fixed_t param);
    void            (*SetAngle)(int type, uint index, uint prop, angle_t param);
    void            (*SetFloat)(int type, uint index, uint prop, float param);
    void            (*SetDouble)(int type, uint index, uint prop, double param);
    void            (*SetPtr)(int type, uint index, uint prop, void* param);

    void            (*SetBoolv)(int type, uint index, uint prop, boolean* params);
    void            (*SetBytev)(int type, uint index, uint prop, byte* params);
    void            (*SetIntv)(int type, uint index, uint prop, int* params);
    void            (*SetFixedv)(int type, uint index, uint prop, fixed_t* params);
    void            (*SetAnglev)(int type, uint index, uint prop, angle_t* params);
    void            (*SetFloatv)(int type, uint index, uint prop, float* params);
    void            (*SetDoublev)(int type, uint index, uint prop, double* params);
    void            (*SetPtrv)(int type, uint index, uint prop, void* params);

    /* pointer-based write functions */
    void            (*SetBoolp)(void* ptr, uint prop, boolean param);
    void            (*SetBytep)(void* ptr, uint prop, byte param);
    void            (*SetIntp)(void* ptr, uint prop, int param);
    void            (*SetFixedp)(void* ptr, uint prop, fixed_t param);
    void            (*SetAnglep)(void* ptr, uint prop, angle_t param);
    void            (*SetFloatp)(void* ptr, uint prop, float param);
    void            (*SetDoublep)(void* ptr, uint prop, double param);
    void            (*SetPtrp)(void* ptr, uint prop, void* param);

    void            (*SetBoolpv)(void* ptr, uint prop, boolean* params);
    void            (*SetBytepv)(void* ptr, uint prop, byte* params);
    void            (*SetIntpv)(void* ptr, uint prop, int* params);
    void            (*SetFixedpv)(void* ptr, uint prop, fixed_t* params);
    void            (*SetAnglepv)(void* ptr, uint prop, angle_t* params);
    void            (*SetFloatpv)(void* ptr, uint prop, float* params);
    void            (*SetDoublepv)(void* ptr, uint prop, double* params);
    void            (*SetPtrpv)(void* ptr, uint prop, void* params);

    /* index-based read functions */
    boolean         (*GetBool)(int type, uint index, uint prop);
    byte            (*GetByte)(int type, uint index, uint prop);
    int             (*GetInt)(int type, uint index, uint prop);
    fixed_t         (*GetFixed)(int type, uint index, uint prop);
    angle_t         (*GetAngle)(int type, uint index, uint prop);
    float           (*GetFloat)(int type, uint index, uint prop);
    double          (*GetDouble)(int type, uint index, uint prop);
    void*           (*GetPtr)(int type, uint index, uint prop);

    void            (*GetBoolv)(int type, uint index, uint prop, boolean* params);
    void            (*GetBytev)(int type, uint index, uint prop, byte* params);
    void            (*GetIntv)(int type, uint index, uint prop, int* params);
    void            (*GetFixedv)(int type, uint index, uint prop, fixed_t* params);
    void            (*GetAnglev)(int type, uint index, uint prop, angle_t* params);
    void            (*GetFloatv)(int type, uint index, uint prop, float* params);
    void            (*GetDoublev)(int type, uint index, uint prop, double* params);
    void            (*GetPtrv)(int type, uint index, uint prop, void* params);

    /* pointer-based read functions */
    boolean         (*GetBoolp)(void* ptr, uint prop);
    byte            (*GetBytep)(void* ptr, uint prop);
    int             (*GetIntp)(void* ptr, uint prop);
    fixed_t         (*GetFixedp)(void* ptr, uint prop);
    angle_t         (*GetAnglep)(void* ptr, uint prop);
    float           (*GetFloatp)(void* ptr, uint prop);
    double          (*GetDoublep)(void* ptr, uint prop);
    void*           (*GetPtrp)(void* ptr, uint prop);

    void            (*GetBoolpv)(void* ptr, uint prop, boolean* params);
    void            (*GetBytepv)(void* ptr, uint prop, byte* params);
    void            (*GetIntpv)(void* ptr, uint prop, int* params);
    void            (*GetFixedpv)(void* ptr, uint prop, fixed_t* params);
    void            (*GetAnglepv)(void* ptr, uint prop, angle_t* params);
    void            (*GetFloatpv)(void* ptr, uint prop, float* params);
    void            (*GetDoublepv)(void* ptr, uint prop, double* params);
    void            (*GetPtrpv)(void* ptr, uint prop, void* params);
}
DENG_API_T(Map);

#ifndef DENG_NO_API_MACROS_MAP
#define DMU_GetType     _api_Map.GetType
#define P_ToIndex     _api_Map.ToIndex
#define P_ToPtr     _api_Map.ToPtr
#define P_Callback     _api_Map.Callback
#define P_Callbackp     _api_Map.Callbackp
#define P_Iteratep     _api_Map.Iteratep
#define P_AllocDummy     _api_Map.AllocDummy
#define P_FreeDummy     _api_Map.FreeDummy
#define P_IsDummy     _api_Map.IsDummy
#define P_DummyExtraData     _api_Map.DummyExtraData
#define P_CountGameMapObjs     _api_Map.CountGameMapObjs
#define P_GetGMOByte     _api_Map.GetGMOByte
#define P_GetGMOShort     _api_Map.GetGMOShort
#define P_GetGMOInt     _api_Map.GetGMOInt
#define P_GetGMOFixed     _api_Map.GetGMOFixed
#define P_GetGMOAngle     _api_Map.GetGMOAngle
#define P_GetGMOFloat     _api_Map.GetGMOFloat
#define P_SetBool     _api_Map.SetBool
#define P_SetByte     _api_Map.SetByte
#define P_SetInt     _api_Map.SetInt
#define P_SetFixed     _api_Map.SetFixed
#define P_SetAngle     _api_Map.SetAngle
#define P_SetFloat     _api_Map.SetFloat
#define P_SetDouble     _api_Map.SetDouble
#define P_SetPtr     _api_Map.SetPtr
#define P_SetBoolv     _api_Map.SetBoolv
#define P_SetBytev     _api_Map.SetBytev
#define P_SetIntv     _api_Map.SetIntv
#define P_SetFixedv     _api_Map.SetFixedv
#define P_SetAnglev     _api_Map.SetAnglev
#define P_SetFloatv     _api_Map.SetFloatv
#define P_SetDoublev     _api_Map.SetDoublev
#define P_SetPtrv     _api_Map.SetPtrv
#define P_SetBoolp     _api_Map.SetBoolp
#define P_SetBytep     _api_Map.SetBytep
#define P_SetIntp     _api_Map.SetIntp
#define P_SetFixedp     _api_Map.SetFixedp
#define P_SetAnglep     _api_Map.SetAnglep
#define P_SetFloatp     _api_Map.SetFloatp
#define P_SetDoublep     _api_Map.SetDoublep
#define P_SetPtrp     _api_Map.SetPtrp
#define P_SetBoolpv     _api_Map.SetBoolpv
#define P_SetBytepv     _api_Map.SetBytepv
#define P_SetIntpv     _api_Map.SetIntpv
#define P_SetFixedpv     _api_Map.SetFixedpv
#define P_SetAnglepv     _api_Map.SetAnglepv
#define P_SetFloatpv     _api_Map.SetFloatpv
#define P_SetDoublepv     _api_Map.SetDoublepv
#define P_SetPtrpv     _api_Map.SetPtrpv
#define P_GetBool     _api_Map.GetBool
#define P_GetByte     _api_Map.GetByte
#define P_GetInt     _api_Map.GetInt
#define P_GetFixed     _api_Map.GetFixed
#define P_GetAngle     _api_Map.GetAngle
#define P_GetFloat     _api_Map.GetFloat
#define P_GetDouble     _api_Map.GetDouble
#define P_GetPtr     _api_Map.GetPtr
#define P_GetBoolv     _api_Map.GetBoolv
#define P_GetBytev     _api_Map.GetBytev
#define P_GetIntv     _api_Map.GetIntv
#define P_GetFixedv     _api_Map.GetFixedv
#define P_GetAnglev     _api_Map.GetAnglev
#define P_GetFloatv     _api_Map.GetFloatv
#define P_GetDoublev     _api_Map.GetDoublev
#define P_GetPtrv     _api_Map.GetPtrv
#define P_GetBoolp     _api_Map.GetBoolp
#define P_GetBytep     _api_Map.GetBytep
#define P_GetIntp     _api_Map.GetIntp
#define P_GetFixedp     _api_Map.GetFixedp
#define P_GetAnglep     _api_Map.GetAnglep
#define P_GetFloatp     _api_Map.GetFloatp
#define P_GetDoublep     _api_Map.GetDoublep
#define P_GetPtrp     _api_Map.GetPtrp
#define P_GetBoolpv     _api_Map.GetBoolpv
#define P_GetBytepv     _api_Map.GetBytepv
#define P_GetIntpv     _api_Map.GetIntpv
#define P_GetFixedpv     _api_Map.GetFixedpv
#define P_GetAnglepv     _api_Map.GetAnglepv
#define P_GetFloatpv     _api_Map.GetFloatpv
#define P_GetDoublepv     _api_Map.GetDoublepv
#define P_GetPtrpv     _api_Map.GetPtrpv
#endif

#ifdef __DOOMSDAY__
DENG_USING_API(Map);
#endif

///@}

#ifdef __cplusplus
} // extern "C"
#endif

///@}

#endif // __DOOMSDAY_MAP_H__
