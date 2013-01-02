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
    DE_API_DIRECT_DATA_ACCESS_v1 = 100,
    DE_API_URI_v1                = 200,
    DE_API_WAD_v1                = 300
};

/**
 * Base structure for API structs.
 */
typedef struct de_api_s {
    int id; ///< API identification (including version) number.
} de_api_t;

#define DENG_DECLARE_API(Name, Prefix) de_api_##Name##_t _api_##Prefix
#define DENG_USING_API(Name, Prefix) DENG_EXTERN_C DENG_DECLARE_API(Name, Prefix)
#define DENG_API_EXCHANGE(APIs) \
    extern "C" void deng_API(int id, void *api) { \
        switch(id) { APIs \
        default: break; } }
#define DENG_GET_API(Ident, Prefix) \
    case Ident: \
        memcpy(&_api_##Prefix, api, sizeof(_api_##Prefix)); \
        break;

#endif // DOOMSDAY_API_BASE_H
