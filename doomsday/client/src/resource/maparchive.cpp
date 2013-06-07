/** @file resource/maparchive.cpp (Cached) Map Archive.
 *
 * @authors Copyright &copy; 2007-2013 Daniel Swanson <danij@dengine.net>
 *
 * Ideas for improvement:
 *
 * "background loading" - it would be very cool if map loading happened in
 * another thread. This way we could be keeping busy while players watch the
 * intermission animations.
 *
 * "seamless world" - multiple concurrent maps with no perceivable delay when
 * players move between them.
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

#include <de/Log>

#include "de_base.h"
#include "de_edit.h"
#include "de_filesys.h"
#include "world/map.h"

#include "resource/maparchive.h"

using namespace de;

/*
// Should we be caching successfully loaded maps?
byte mapCache = true;

static char const *mapCacheDir = "mapcache/";
*/

static inline lumpnum_t markerLumpNumForPath(String path)
{
    return App_FileSystem().lumpNumForName(path);
}

static String composeUniqueMapId(de::File1 &markerLump)
{
    return String("%1|%2|%3|%4")
              .arg(markerLump.name().fileNameWithoutExtension())
              .arg(markerLump.container().name().fileNameWithoutExtension())
              .arg(markerLump.container().hasCustom()? "pwad" : "iwad")
              .arg(Str_Text(App_CurrentGame().identityKey()))
              .toLower();
}

#if 0
/// Calculate the identity key for maps loaded from the specified @a path.
static ushort calculateIdentifierForMapPath(char const *path)
{
    DENG_ASSERT(path && path[0]);

    ushort identifier = 0;
    for(uint i = 0; path[i]; ++i)
    {
        identifier ^= path[i] << ((i * 3) % 11);
    }
    return identifier;
}

/**
 * Compose the relative path (relative to the runtime directory) to the directory
 * within the archived map cache where maps from the specified source will reside.
 *
 * @param sourcePath  Path to the primary resource file for the original map data.
 * @return  The composed path.
 */
static AutoStr *mapCachePath(char const *path)
{
    if(!path || !path[0]) return 0;

    DENG_ASSERT(App_GameLoaded());

    ddstring_t const *gameIdentityKey = App_CurrentGame().identityKey();
    ushort mapPathIdentifier   = calculateIdentifierForMapPath(path);

    AutoStr *mapFileName = AutoStr_NewStd();
    F_FileName(mapFileName, path);

    // Compose the final path.
    AutoStr *path = AutoStr_NewStd();
    Str_Appendf(path, "%s%s/%s-%04X/", mapCacheDir, Str_Text(gameIdentityKey),
                                       Str_Text(mapFileName), mapPathIdentifier);
    F_ExpandBasePath(path, path);

    return path;
}
#endif

MapArchive::Info::Info(de::Uri const &mapUri/*, ddstring_t const *cachedMapPath*/)
    : _uri(mapUri)/*,
      cachedMapFound(false),
      lastLoadAttemptFailed(false)*/
{
    /*Str_Init(&cachedMapPath);
    Str_Set(&cachedMapPath, Str_Text(cachedMapPath));*/
}

MapArchive::Info::~Info()
{
    //Str_Free(&cachedMapPath);
}

de::Uri MapArchive::Info::mapUri() const
{
    return _uri;
}

Map *MapArchive::Info::loadMap(/*bool forceRetry = false*/)
{
    //if(lastLoadAttemptFailed && !forceRetry)
    //    return false;

    //lastLoadAttemptFailed = false;

    // Load from cache?
    /*if(mapCache && cachedMapFound)
    {
        if(Map *map = loadCachedMap())
            return map;

        lastLoadAttemptFailed = true;
        return 0;
    }*/

    // Try a JIT conversion with the help of a plugin.
    if(Map *map = convert())
        return map;

    LOG_WARNING("Failed conversion of \"%s\".") << _uri;
    //lastLoadAttemptFailed = true;
    return 0;
}

#if 0
bool MapArchive::Info::isCachedMapDataAvailable()
{
    if(DAM_MapIsValid(Str_Text(&cachedMapPath), markerLumpNum()))
    {
        cachedMapFound = true;
    }
    return cachedMapFound;
}

Map *MapArchive::Info::loadCachedMap()
{
    Map *map = DAM_MapRead(Str_Text(&cachedMapPath));
    if(!map) return 0;

    map->_uri = _uri;
    return map;
}
#endif

Map *MapArchive::Info::convert()
{
    LOG_AS("MapArchive::convert");

    // At least one available converter?
    if(!Plug_CheckForHook(HOOK_MAP_CONVERT))
        return 0;

    //LOG_DEBUG("Attempting \"%s\"...") << _uri;

    lumpnum_t markerLumpNum = markerLumpNumForPath(_uri.path());
    if(markerLumpNum < 0)
        return 0;

    // Ask each converter in turn whether the map format is
    // recognizable and if so to interpret and transfer it to us
    // via the map editing interface.
    if(!DD_CallHooks(HOOK_MAP_CONVERT, 0, (void *) reinterpret_cast<uri_s *>(&_uri)))
        return 0;

    // A converter signalled success.
    // Were we able to produce a valid map from the data it provided?
    if(!MPE_GetLastBuiltMapResult())
        return 0;

    Map *map = MPE_GetLastBuiltMap();
    DENG_ASSERT(map != 0);

    map->_uri = _uri;

    // Generate the unique map id.
    de::File1 &markerLump   = App_FileSystem().nameIndex().lump(markerLumpNum);
    String uniqueId         = composeUniqueMapId(markerLump);
    QByteArray uniqueIdUtf8 = uniqueId.toUtf8();
    qstrncpy(map->_oldUniqueId, uniqueIdUtf8.constData(), sizeof(map->_oldUniqueId));

    // Are we caching this map?
    /*if(mapCache)
    {
        AutoStr *cachedMapDir =
            MapArchive_MapCachePath(Str_Text(F_ComposeLumpFilePath(markerLumpNum())));

        AutoStr *cachedMapPath = AutoStr_NewStd();
        F_FileName(cachedMapPath, F_LumpName(markerLumpName));

        Str_Append(cachedMapPath, ".dcm");
        Str_Prepend(cachedMapPath, Str_Text(cachedMapDir));
        F_ExpandBasePath(cachedMapPath, cachedMapPath);

        // Ensure the destination directory exists.
        F_MakePath(Str_Text(cachedMapDir));

        // Cache the map!
        DAM_MapWrite(map, Str_Text(cachedMapPath));
    }*/

    return map;
}

DENG2_PIMPL(MapArchive)
{
public:
    MapArchive::Infos infos;

    Instance(Public *i) : Base(i) {}

    ~Instance() { self.clear(); }
};

MapArchive::MapArchive() : d(new Instance(this))
{}

void MapArchive::consoleRegister() // static
{
    //C_VAR_BYTE("map-cache", &mapCache, 0, 0, 1);
}

void MapArchive::clear()
{
    foreach(Info *info, d->infos) { delete info; }
    d->infos.clear();
}

MapArchive::Info *MapArchive::findInfo(de::Uri const &uri) const
{
    foreach(Info *info, d->infos)
    {
        // Is this the info we are looking for?
        if(uri == info->mapUri())
            return info;
    }
    return 0; // Not found.
}

MapArchive::Info &MapArchive::createInfo(de::Uri const &uri)
{
    // Do we have existing info for this?
    if(Info *info = findInfo(uri))
        return *info;

    /*
    // Compose the cache directory path and ensure it exists.
    AutoStr *cachedMapPath = AutoStr_NewStd();
    lumpnum_t markerLumpNum = F_LumpNumForName(uri.path().toString().toLatin1().constData());
    if(markerLumpNum >= 0)
    {
        AutoStr *cachedMapDir = MapArchive_MapCachePath(Str_Text(F_ComposeLumpFilePath(markerLumpNum)));
        F_MakePath(Str_Text(cachedMapDir));

        // Compose the full path to the cached map data file.
        F_FileName(cachedMapPath, Str_Text(F_LumpName(markerLumpNum)));
        Str_Append(cachedMapPath, ".dcm");
        Str_Prepend(cachedMapPath, Str_Text(cachedMapDir));
    }
    */

    d->infos.append(new Info(uri/*, ddstring_t const *cachedMapPath*/));
    return *d->infos.last();
}

MapArchive::Infos const &MapArchive::infos() const
{
    return d->infos;
}
