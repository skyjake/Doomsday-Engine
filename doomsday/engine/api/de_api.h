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

/// All APIs exported from the executable.
enum {
    DE_API_DEFINITIONS_v1        = 100,     // 1.10
    DE_API_DIRECT_DATA_ACCESS_v1 = 200,     // 1.10
    DE_API_PLUGIN_v1             = 300,     // 1.10
    DE_API_URI_v1                = 400,     // 1.10
    DE_API_WAD_v1                = 500      // 1.10
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
    extern "C" void deng_API(int id, void *api) { \
        switch(id) { APIs \
        default: break; } }
#define DENG_GET_API(Ident, Name) \
    case Ident: \
        memcpy(&_api_##Name, api, sizeof(_api_##Name)); \
        break;

#endif // DOOMSDAY_API_BASE_H
