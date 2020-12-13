/** @file apis.h  Doomsday's public API mechanism.
 * @ingroup base
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2014-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef RC_INVOKED
#  include <de/liblegacy.h>
#  include <string.h>
#endif

/**
 * All APIs exported from the executable.
 *
 * @par Freezing policy
 *
 * If changes are made to an API after it has been included in a stable
 * release, a new version of the API must be added. If feasible, the old
 * version of the API should be published in addition to the new version. Note
 * that it is possible to add completely new APIs without affecting the
 * existing ones.
 *
 * @par Adding a new API version
 *
 * Add a new enumeration into the table below for the API in question,
 * incrementing by one from the previous value of the API version. For
 * instance: <pre>DE_API_MAP_v2 = 1101, // 1.11</pre>
 *
 * Leave the existing enumerations as-is: if old plugins request obsolete API
 * versions from the engine, the engine should be able to identify which API is
 * being asked. It is also useful for developers to have a historical record of
 * which API version was in which release.
 *
 * Update the 'versionless'/latest API definition (e.g., DE_API_MAP) to the
 * value of the latest version of that particular API. This will automatically
 * cause the engine to export the latest version and all of the project's
 * plugins are also then using the latest version.
 *
 * When it is necessary to also export an older version of an API for backwards
 * compatibility, it must be manually added to the set of published APIs (see
 * library.cpp) and the API struct in question should be copied to a separate
 * header file for that version of the API (e.g., api_map_v1.h).
 *
 * @par Deprecating/removing an API
 *
 * When an API becomes fully obsolete, a new version with a "_REMOVED" suffix should be
 * added. This new enum should have a new identification number to differentiate it from
 * previous valid versions. As always, the old enums of the API should be retained for
 * historical reasons. In the future, possible new APIs should not use the same
 * identification numbers. A removed API does not need a 'versionless' enumeration,
 * though, as there is no more latest version of the API available.
 */
enum {
    DE_API_BASE_v1              = 0,       // 1.10
    DE_API_BASE_v2              = 1,       // 1.14
    DE_API_BASE_v3              = 2,       // 2.0
    DE_API_BASE_v4              = 3,       // 2.1
    DE_API_BASE                 = DE_API_BASE_v4,

    DE_API_BINDING_v1           = 100,     // 1.10
    DE_API_BINDING_v2           = 101,     // 1.15
    DE_API_BINDING              = DE_API_BINDING_v2,

    DE_API_BUSY_v1              = 200,     // 1.10
    DE_API_BUSY_v2              = 201,     // 1.13
    DE_API_BUSY_v3              = 202,     // 2.0
    DE_API_BUSY                 = DE_API_BUSY_v3,

    DE_API_CLIENT_v1            = 300,     // 1.10
    DE_API_CLIENT               = DE_API_CLIENT_v1,

    DE_API_CONSOLE_v1           = 400,     // 1.10
    DE_API_CONSOLE_v2           = 401,     // 1.14
    DE_API_CONSOLE              = DE_API_CONSOLE_v2,

    DE_API_DEFINITIONS_v1       = 500,     // 1.10
    DE_API_DEFINITIONS_v2       = 501,     // 1.15
    DE_API_DEFINITIONS_v3       = 502,     // 2.0
    DE_API_DEFINITIONS          = DE_API_DEFINITIONS_v3,

    DE_API_FILE_SYSTEM_v1       = 600,     // 1.10
    DE_API_FILE_SYSTEM_v2       = 601,     // 1.14
    DE_API_FILE_SYSTEM_v3       = 602,     // 1.15
    DE_API_FILE_SYSTEM_v4       = 603,     // 2.0
    DE_API_FILE_SYSTEM          = DE_API_FILE_SYSTEM_v4,

    DE_API_FONT_RENDER_v1       = 700,     // 1.10
    DE_API_FONT_RENDER          = DE_API_FONT_RENDER_v1,

    DE_API_GL_v1                = 800,     // 1.10
    DE_API_GL_v2                = 801,     // 1.13
    DE_API_GL_v3                = 802,     // 1.15
    DE_API_GL_v4                = 803,     // 2.1
    DE_API_GL                   = DE_API_GL_v4,

    DE_API_INFINE_v1            = 900,     // 1.10
    DE_API_INFINE               = DE_API_INFINE_v1,

    DE_API_INTERNAL_DATA_v1     = 1000,    // 1.10
    DE_API_INTERNAL_DATA_v2     = 1001,    // 2.0 (removed sprNames)
    DE_API_INTERNAL_DATA        = DE_API_INTERNAL_DATA_v2,

    DE_API_MAP_v1               = 1100,    // 1.10
    DE_API_MAP_v2               = 1101,    // 1.11
    DE_API_MAP_v3               = 1102,    // 1.13
    DE_API_MAP_v4               = 1103,    // 1.15
    DE_API_MAP_v5               = 1104,    // 2.0
    DE_API_MAP = DE_API_MAP_v5,

    DE_API_MAP_EDIT_v1          = 1200,    // 1.10
    DE_API_MAP_EDIT_v2          = 1201,    // 1.11
    DE_API_MAP_EDIT_v3          = 1202,    // 2.0
    DE_API_MAP_EDIT_v4          = 1203,    // 2.3
    DE_API_MAP_EDIT = DE_API_MAP_EDIT_v4,

    DE_API_MATERIALS_v1         = 1300,    // 1.10
    DE_API_MATERIALS            = DE_API_MATERIALS_v1,

    DE_API_MATERIAL_ARCHIVE_v1  = 1400,    // 1.10
    DE_API_MATERIAL_ARCHIVE_REMOVED
                                = 1401,    // 2.0 (API removed)

    DE_API_PLAYER_v1            = 1500,    // 1.10
    DE_API_PLAYER_v2            = 1501,    // 1.13
    DE_API_PLAYER               = DE_API_PLAYER_v2,

    DE_API_PLUGIN_v1            = 1600,    // 1.10
    DE_API_PLUGIN_REMOVED       = 1601,    // 2.0 (API removed)

    DE_API_RENDER_v1            = 1700,    // 1.10
    DE_API_RENDER               = DE_API_RENDER_v1,

    DE_API_RESOURCE_v1          = 1800,    // 1.10
    DE_API_RESOURCE_v2          = 1801,    // 1.13
    DE_API_RESOURCE             = DE_API_RESOURCE_v2,

    DE_API_SERVER_v1            = 1900,    // 1.10
    DE_API_SERVER               = DE_API_SERVER_v1,

    DE_API_SOUND_v1             = 2000,    // 1.10
    DE_API_SOUND_v2             = 2001,    // 2.0
    DE_API_SOUND                = DE_API_SOUND_v2,

    DE_API_SVG_v1               = 2100,    // 1.10
    DE_API_SVG                  = DE_API_SVG_v1,

    DE_API_THINKER_v1           = 2200,    // 1.10
    DE_API_THINKER_v2           = 2201,    // 1.15
    DE_API_THINKER              = DE_API_THINKER_v2,

    DE_API_URI_v1               = 2300,    // 1.10
    DE_API_URI_v2               = 2301,    // 1.14
    DE_API_URI                  = DE_API_URI_v2,

    DE_API_WAD_v1               = 2400,    // 1.10
    DE_API_WAD_v2               = 2401,    // 1.14
    DE_API_WAD_REMOVED          = 2402     // 1.15 (API removed)
};

/**
 * Base structure for API structs.
 */
typedef struct de_api_s {
    int id;  ///< API identification (including version) number.
} de_api_t;

#define DENG_API_TYPEDEF(Name)  typedef struct de_api_##Name##_s
#define DENG_API_T(Name)        de_api_##Name##_t
#define DENG_DECLARE_API(Name)  DENG_API_T(Name) _api_##Name
#define DENG_USING_API(Name)    DENG_EXTERN_C DENG_DECLARE_API(Name)

#define DENG_API_EXCHANGE(APIs) \
    DENG_EXTERN_C void deng_API(int id, void *api) { \
        switch(id) { APIs \
        default: break; } }
#define DENG_GET_API(Ident, Name) \
    case Ident: \
        memcpy(&_api_##Name, api, sizeof(_api_##Name)); \
        DENG_ASSERT(_api_##Name.api.id == Ident); \
        break;

#if defined (DENG_STATIC_LINK)
#define DENG_SYMBOL_PTR(var, symbolName) \
    if (!qstrcmp(var, #symbolName)) { \
        return reinterpret_cast<void *>(symbolName); \
    }
#endif

#endif  // DOOMSDAY_APIS_H
