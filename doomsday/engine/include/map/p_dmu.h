/** @file p_dmu.h Doomsday Map Update API
 *
 * The Map Update API is used for accessing and making changes to map data
 * during gameplay. From here, the relevant engine's subsystems will be
 * notified of changes in the map data they use, thus allowing them to
 * update their status whenever needed.
 *
 * @author Copyright &copy; 2006-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MAP_UPDATE_H
#define LIBDENG_MAP_UPDATE_H

#include "api_mapedit.h" // valuetype_t

#ifdef __cplusplus
extern "C" {
#endif

typedef struct setargs_s {
    int type;
    uint prop;
    int modifiers; /// Property modifiers (e.g., line of sector)
    valuetype_t valueType;
    boolean *booleanValues;
    byte *byteValues;
    int *intValues;
    fixed_t *fixedValues;
    float *floatValues;
    double *doubleValues;
    angle_t *angleValues;
    void **ptrValues;
} setargs_t;

/**
 * Initializes the dummy arrays with a fixed number of dummies.
 */
void P_InitMapUpdate(void);

/**
 * Allocates a new dummy object.
 *
 * @param type          DMU type of the dummy object.
 * @param extraData     Extra data pointer of the dummy. Points to
 *                      caller-allocated memory area of extra data for the
 *                      dummy.
 */
//void *P_AllocDummy(int type, void *extraData);

/**
 * Determines the type of a dummy object. For extra safety (in a debug build)
 * it would be possible to look through the dummy arrays and make sure the
 * pointer refers to a real dummy.
 */
int P_DummyType(void *dummy);

/**
 * Frees a dummy object.
 */
//void P_FreeDummy(void *dummy);

/**
 * Determines if a map data object is a dummy.
 */
//boolean P_IsDummy(void *dummy);

/**
 * Returns the extra data pointer of the dummy, or NULL if the object is not
 * a dummy object.
 */
//void *P_DummyExtraData(void *dummy);

/**
 * Convert pointer to index.
 */
//uint P_ToIndex(void const *ptr);

/**
 * Convert DMU enum constant into a string for error/debug messages.
 */
char const *DMU_Str(uint prop);

//int DMU_GetType(void const *ptr);

/**
 * Sets a value. Does some basic type checking so that incompatible types are
 * not assigned. Simple conversions are also done, e.g., float to fixed.
 */
void DMU_SetValue(valuetype_t valueType, void *dst, setargs_t const *args, uint index);

/**
 * Gets a value. Does some basic type checking so that incompatible types
 * are not assigned. Simple conversions are also done, e.g., float to
 * fixed.
 */
void DMU_GetValue(valuetype_t valueType, void const *src, setargs_t *args, uint index);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_MAP_UPDATE_H
