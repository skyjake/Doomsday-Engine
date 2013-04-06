/** @file dam_main.cpp (Cached) Map Archive.
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
#include "map/gamemap.h"

#include "map/dam_main.h"

using namespace de;

/*
// Should we be caching successfully loaded maps?
byte mapCache = true;

static char const *mapCacheDir = "mapcache/";
*/

void MapArchive_Register()
{
    //C_VAR_BYTE("map-cache", &mapCache, 0, 0, 1);
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
#endif

class MapArchive
{
public:
    /**
     * Information about a map in the archive.
     */
    class Info
    {
        de::Uri _uri;
        /*ddstring_t cachedMapPath;
        bool cachedMapFound;
        bool lastLoadAttemptFailed;*/

#ifdef MACOS_10_4
        // GCC 4.0 on Mac OS X 10.5 doesn't handle nested classes
        // and friends that well.
    public:
#endif
        Info(de::Uri const &mapUri/*, ddstring_t const *cachedMapPath*/)
            : _uri(mapUri)/*,
              cachedMapFound(false),
              lastLoadAttemptFailed(false)*/
        {
            /*Str_Init(&cachedMapPath);
            Str_Set(&cachedMapPath, Str_Text(cachedMapPath));*/
        }

        ~Info()
        {
            //Str_Free(&cachedMapPath);
        }

    public:
        de::Uri mapUri() const
        {
            return _uri;
        }

        /**
         * Attempt to load the associated map data.
         *
         * @return  Pointer to the loaded map; otherwise @c 0.
         */
        GameMap *loadMap(/*bool override = false*/)
        {
            //if(lastLoadAttemptFailed && !override)
            //    return false;

            //lastLoadAttemptFailed = false;

            // Load from cache?
            /*if(mapCache && cachedMapFound)
            {
                if(GameMap *map = loadCachedMap())
                    return map;

                lastLoadAttemptFailed = true;
                return 0;
            }*/

            // Try a JIT conversion with the help of a plugin.
            if(GameMap *map = convertMap())
                return map;

            LOG_WARNING("Failed conversion of \"%s\".") << _uri;
            //lastLoadAttemptFailed = true;
            return 0;
        }

        friend class MapArchive;

    private:
        inline lumpnum_t markerLumpNumForPath()
        {
            return App_FileSystem().lumpNumForName(_uri.path());
        }

#if 0
        bool isCachedMapDataAvailable()
        {
            if(DAM_MapIsValid(Str_Text(&cachedMapPath), markerLumpNum()))
            {
                cachedMapFound = true;
            }
            return cachedMapFound;
        }

        GameMap *loadCachedMap()
        {
            GameMap *map = DAM_MapRead(Str_Text(&cachedMapPath));
            if(!map) return 0;

            map->_uri = _uri;
            return map;
        }
#endif

        GameMap *convertMap()
        {
            LOG_AS("ArchivedMapInfo::convertMap");

            // At least one available converter?
            if(!Plug_CheckForHook(HOOK_MAP_CONVERT))
                return 0;

            LOG_VERBOSE("Attempting \"%s\"...") << _uri;

            lumpnum_t markerLumpNum = markerLumpNumForPath();
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

            GameMap *map = MPE_GetLastBuiltMap();
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
    };

    typedef QList<Info *> Infos;

public:
    MapArchive() {}
    ~MapArchive() { clear(); }

    void clear()
    {
        foreach(Info *info, _infos) { delete info; }
        _infos.clear();
    }

    /**
     * Attempt to locate the info for an archived map by URI.
     *
     * @param uri  Identifier of the info to be located.
     *
     * @return  Pointer to the found info; otherwise @c 0.
     */
    Info *findByUri(de::Uri const &uri)
    {
        foreach(Info *info, _infos)
        {
            // Is this the info we are looking for?
            if(uri == info->mapUri())
                return info;
        }
        return 0; // Not found.
    }

    Info &createInfo(de::Uri const &uri)
    {
        // Do we have existing info for this?
        if(Info *info = findByUri(uri))
            return *info;

        LOG_DEBUG("Adding new ArchivedMapInfo for '%s'.") << uri;

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

        _infos.append(new Info(uri/*, ddstring_t const *cachedMapPath*/));
        return *_infos.last();
    }

private:
    Infos _infos;
};

static MapArchive archive;

void MapArchive_Initialize()
{
    // Allow re-init.
    archive.clear();
}

void MapArchive_Shutdown()
{
    archive.clear();
}

GameMap *MapArchive_LoadMap(de::Uri const &uri)
{
    // Record this map if we haven't already and load then it in!
    return archive.createInfo(uri).loadMap();
}

#if 0
AutoStr *MapArchive_MapCachePath(char const *sourcePath)
{
    if(!sourcePath || !sourcePath[0]) return 0;

    DENG_ASSERT(App_GameLoaded());

    ddstring_t const *gameIdentityKey = App_CurrentGame().identityKey();
    ushort mapPathIdentifier   = calculateIdentifierForMapPath(sourcePath);

    AutoStr *mapFileName = AutoStr_NewStd();
    F_FileName(mapFileName, sourcePath);

    // Compose the final path.
    AutoStr *path = AutoStr_NewStd();
    Str_Appendf(path, "%s%s/%s-%04X/", mapCacheDir, Str_Text(gameIdentityKey),
                                       Str_Text(mapFileName), mapPathIdentifier);
    F_ExpandBasePath(path, path);

    return path;
}
#endif
