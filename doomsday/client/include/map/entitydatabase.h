/**
 * @file entitydatabase.h
 * Entity property value database. @ingroup map
 *
 * The EntityDatabase is used in the process of transferring mobj spawn spot
 * information and stuff like line action specials from the wad map loader
 * plugin via the engine, through to the game plugin.
 *
 * The primary reason for its existence is that the engine does not know about
 * the game specific properties of the map data types. The engine does not care
 * at all about the values or indeed even what properties are registered; it is
 * simply a way of piping information from one part of the system to another.
 *
 * @todo C++ allows making this even more generic: a set/map of polymorphic objects.
 *
 * @author Copyright &copy; 2007-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MAP_ENTITYDATABASE_H
#define LIBDENG_MAP_ENTITYDATABASE_H

#ifdef __cplusplus
class PropertyValue;

extern "C" {
#endif

/**
 * C Wrapper API:
 */

struct mapentitydef_s;
struct mapentitypropertydef_s;

struct entitydatabase_s;
typedef struct entitydatabase_s EntityDatabase;

EntityDatabase* EntityDatabase_New(void);

void EntityDatabase_Delete(EntityDatabase* db);

uint EntityDatabase_EntityCount(EntityDatabase* db, struct mapentitydef_s* entityDef);

boolean EntityDatabase_HasEntity(EntityDatabase* db, struct mapentitydef_s* entityDef, uint elementIndex);

#ifdef __cplusplus
PropertyValue const* EntityDatabase_Property(EntityDatabase* db, struct mapentitypropertydef_s* propertyDef, uint elementIndex);
#endif

boolean EntityDatabase_SetProperty(EntityDatabase* db, struct mapentitypropertydef_s* propertyDef, uint elementIndex, valuetype_t valueType, void* valueAdr);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_MAP_ENTITYDATABASE_H
