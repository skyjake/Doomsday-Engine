/** @file de_api.h Doomsday's public API mechanism.
 * @ingroup base
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DOOMSDAY_API_BASE_H
#define DOOMSDAY_API_BASE_H

#include <de/libdeng.h>

/// All APIs exported from the executable.
enum {
    DE_API_BASE_v1               = 0,       // 1.10
    DE_API_BUSY_v1               = 100,     // 1.10
    DE_API_CONSOLE_v1            = 200,     // 1.10
    DE_API_DEFINITIONS_v1        = 300,     // 1.10
    DE_API_DIRECT_DATA_ACCESS_v1 = 400,     // 1.10
    DE_API_FILE_SYSTEM_v1        = 500,     // 1.10
    DE_API_MAP_EDIT_v1           = 600,     // 1.10
    DE_API_MATERIALS_v1          = 700,     // 1.10
    DE_API_PLUGIN_v1             = 800,     // 1.10
    DE_API_URI_v1                = 900,     // 1.10
    DE_API_WAD_v1                = 1000     // 1.10
};

/**
 * Base structure for API structs.
 */
typedef struct de_api_s {
    int id; ///< API identification (including version) number.
} de_api_t;

#define DENG_API_TYPEDEF(Name) typedef struct de_api_##Name##_s
#define DENG_API_T(Name) de_api_##Name##_t
#define DENG_DECLARE_API(Name) DENG_API_T(Name) _api_##Name
#define DENG_USING_API(Name) DENG_EXTERN_C DENG_DECLARE_API(Name)
#define DENG_API_EXCHANGE(APIs) \
    DENG_EXTERN_C void deng_API(int id, void *api) { \
        switch(id) { APIs \
        default: break; } }
#define DENG_GET_API(Ident, Name) \
    case Ident: \
        memcpy(&_api_##Name, api, sizeof(_api_##Name)); \
        DENG_ASSERT(_api_##Name.api.id == Ident); \
        break;

// The Base API.
DENG_API_TYPEDEF(Base) // v1
{
    de_api_t api;

    int (*GetInteger)(int ddvalue);
    void (*SetInteger)(int ddvalue, int parm);
    void* (*GetVariable)(int ddvalue);
    void (*SetVariable)(int ddvalue, void* ptr);
}
DENG_API_T(Base);

#ifndef DENG_NO_API_MACROS_BASE
#define DD_GetInteger   _api_Base.GetInteger
#define DD_SetInteger   _api_Base.SetInteger
#define DD_GetVariable  _api_Base.GetVariable
#define DD_SetVariable  _api_Base.SetVariable
#endif

#ifdef __DOOMSDAY__
DENG_USING_API(Base);
#endif

#endif // DOOMSDAY_API_BASE_H
