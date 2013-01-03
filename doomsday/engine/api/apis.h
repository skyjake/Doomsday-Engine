/** @file api/apis.h Doomsday's public API mechanism.
 * @ingroup base
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DOOMSDAY_APIS_H
#define DOOMSDAY_APIS_H

#include <de/libdeng.h>

/// All APIs exported from the executable.
enum {
    DE_API_BASE_v1               = 0,       // 1.10
    DE_API_BASE_latest           = DE_API_BASE_v1,

    DE_API_BINDING_v1            = 100,     // 1.10
    DE_API_BINDING_latest        = DE_API_BINDING_v1,

    DE_API_BUSY_v1               = 200,     // 1.10
    DE_API_BUSY_latest           = DE_API_BUSY_v1,

    DE_API_CLIENT_v1             = 300,     // 1.10
    DE_API_CLIENT_latest         = DE_API_CLIENT_v1,

    DE_API_CONSOLE_v1            = 400,     // 1.10
    DE_API_CONSOLE_latest        = DE_API_CONSOLE_v1,

    DE_API_DEFINITIONS_v1        = 500,     // 1.10
    DE_API_DEFINITIONS_latest    = DE_API_DEFINITIONS_v1,

    DE_API_DIRECT_DATA_ACCESS_v1 = 600,     // 1.10
    DE_API_DIRECT_DATA_ACCESS_latest = DE_API_DIRECT_DATA_ACCESS_v1,

    DE_API_FILE_SYSTEM_v1        = 700,     // 1.10
    DE_API_FILE_SYSTEM_latest    = DE_API_FILE_SYSTEM_v1,

    DE_API_FONT_RENDER_v1        = 800,     // 1.10
    DE_API_FONT_RENDER_latest    = DE_API_FONT_RENDER_v1,

    DE_API_INFINE_v1             = 900,     // 1.10
    DE_API_INFINE_latest         = DE_API_INFINE_v1,

    DE_API_MAP_v1                = 1000,    // 1.10
    DE_API_MAP_latest            = DE_API_MAP_v1,

    DE_API_MAP_EDIT_v1           = 1100,    // 1.10
    DE_API_MAP_EDIT_latest       = DE_API_MAP_EDIT_v1,

    DE_API_MATERIALS_v1          = 1200,    // 1.10
    DE_API_MATERIALS_latest      = DE_API_MATERIALS_v1,

    DE_API_PLAYER_v1             = 1300,    // 1.10
    DE_API_PLAYER_latest         = DE_API_PLAYER_v1,

    DE_API_PLUGIN_v1             = 1400,    // 1.10
    DE_API_PLUGIN_latest         = DE_API_PLUGIN_v1,

    DE_API_SERVER_v1             = 1500,    // 1.10
    DE_API_SERVER_latest         = DE_API_SERVER_v1,

    DE_API_SOUND_v1              = 1600,    // 1.10
    DE_API_SOUND_latest          = DE_API_SOUND_v1,

    DE_API_SVG_v1                = 1700,    // 1.10
    DE_API_SVG_latest            = DE_API_SVG_v1,

    DE_API_THINKER_v1            = 1800,    // 1.10
    DE_API_THINKER_latest        = DE_API_THINKER_v1,

    DE_API_URI_v1                = 1900,    // 1.10
    DE_API_URI_latest            = DE_API_URI_v1,

    DE_API_WAD_v1                = 2000,    // 1.10
    DE_API_WAD_latest            = DE_API_WAD_v1
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

#endif // DOOMSDAY_APIS_H
