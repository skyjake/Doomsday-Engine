/** @file world/p_mapdata.h World Map Entity Definitions.
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_P_MAPDATA_H
#define DENG_WORLD_P_MAPDATA_H

#include "dd_share.h"
#include "api_mapedit.h"
#include "m_nodepile.h"
#include <de/binangle.h>
#include <de/vector1.h>

struct mapentitydef_s;

typedef struct mapentitypropertydef_s {
    /// Entity-unique identifier associated with this property.
    int id;

    /// Entity-unique name for this property.
    char *name;

    /// Value type identifier for this property.
    valuetype_t type;

    /// Entity definition which owns this property.
    struct mapentitydef_s *entity;
} MapEntityPropertyDef;

/**
 * @ingroup world
 */
typedef struct mapentitydef_s {
    /// Unique identifier associated with this entity.
    int id;

    /// Set of known properties for this entity.
    uint numProps;
    MapEntityPropertyDef *props;

#ifdef __cplusplus
    mapentitydef_s(int _id) : id(_id), numProps(0), props(0) {}
#endif
} MapEntityDef;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Lookup a defined property by identifier.
 *
 * @param def           MapEntityDef instance.
 * @param propertyId    Entity-unique identifier for the property to lookup.
 * @param retDef        If not @c NULL, the found property definition is
 *                      written here (else @c 0 if not found).
 *
 * @return Logical index of the found property (zero-based) else @c -1 if not found.
 */
int MapEntityDef_Property2(MapEntityDef *def, int propertyId,
                           MapEntityPropertyDef **retDef = 0);

/**
 * Lookup a defined property by name.
 *
 * @param def           MapEntityDef instance.
 * @param propertyName  Entity-unique name for the property to lookup.
 * @param retDef        If not @c NULL, the found property definition is
 *                      written here (else @c 0 if not found).
 *
 * @return Logical index of the found property (zero-based) else @c -1 if not found.
 */
int MapEntityDef_PropertyByName(MapEntityDef *def, char const *propertyName,
                                MapEntityPropertyDef **retDef = 0);

/**
 * Lookup a MapEntityDef by unique identfier @a id.
 *
 * @note Performance is O(log n).
 *
 * @return Found MapEntityDef else @c NULL.
 */
MapEntityDef *P_MapEntityDef(int id);

/**
 * Lookup a MapEntityDef by unique name.
 *
 * @note Performance is O(log n).
 *
 * @return Found MapEntityDef else @c NULL.
 */
MapEntityDef *P_MapEntityDefByName(char const *name);

/**
 * Lookup the unique name associated with MapEntityDef @a def.
 *
 * @note Performance is O(n).
 *
 * @return Unique name associated with @a def if found, else a zero-length string.
 */
AutoStr *P_NameForMapEntityDef(MapEntityDef *def);

/**
 * To be called to initialize the game map object defs.
 */
void P_InitMapEntityDefs();

/**
 * To be called to free all memory allocated for the map obj defs.
 */
void P_ShutdownMapEntityDefs();

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DENG_WORLD_P_MAPDATA_H
