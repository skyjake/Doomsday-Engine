/** @file de_def.h Public API for definitions.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef DOOMSDAY_DEF_H
#define DOOMSDAY_DEF_H

#include "api_base.h"

/// @addtogroup defs
///@{

DENG_API_TYPEDEF(Def) // v1
{
    de_api_t api;

    int (*Get)(int type, const char* id, void* out);
    int (*Set)(int type, int index, int value, const void* ptr);
    int (*EvalFlags)(char* flags);

    // Functions related to DED database manipulation (deprecated):
    int (*DED_AddValue)(void *ded, char const* id);
    void (*DED_NewEntries)(void** ptr, void* dedCount, size_t elemSize, int count);
}
DENG_API_T(Def);

#ifndef DENG_NO_API_MACROS_DEFINITIONS
#define Def_Get         _api_Def.Get
#define Def_Set         _api_Def.Set
#define Def_EvalFlags   _api_Def.EvalFlags
#endif

#ifdef __DOOMSDAY__
DENG_USING_API(Def);
#endif

///@}

#endif // DOOMSDAY_DEF_H
