/** @file dam_main.h (Cached) Map Archive.
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

#ifndef LIBDENG_ARCHIVED_MAP_MAIN_H
#define LIBDENG_ARCHIVED_MAP_MAIN_H

#include "dd_share.h"
#include "uri.hh"

class GameMap;

namespace de {

/**
 * @ingroup base
 */
class MapArchive
{
public:
    /**
     * Information about a map in the archive.
     */
    class Info
    {
#ifdef MACOS_10_4
        // GCC 4.0 on Mac OS X 10.5 doesn't handle nested classes
        // and friends that well.
    public:
#endif
        Info(Uri const &mapUri/*, ddstring_t const *cachedMapPath*/);
        ~Info();

    public:
        /**
         * Returns the unqiue identifier for the map.
         */
        Uri mapUri() const;

        /**
         * Attempt to load the associated map data.
         *
         * @return  Pointer to the loaded map; otherwise @c 0.
         */
        GameMap *loadMap(/*bool forceRetry = false*/);

        friend class MapArchive;

    private:
#if 0
        /**
         * Returns @c true iff data for the map is available in the cache.
         */
        bool isCachedMapDataAvailable();

        /**
         * Attempt to load data for the map from the cache.
         *
         * @see isCachedMapDataAvailable()
         *
         * @return  Pointer to the loaded GameMap; otherwise @c 0.
         */
        GameMap *loadCachedMap();
#endif

        /**
         * Attempt to peform a JIT conversion of the map data with the help
         * of a map converter plugin.
         *
         * @return  Pointer to the converted GameMap; otherwise @c 0.
         */
        GameMap *convertMap();

        Uri _uri;
        /*ddstring_t cachedMapPath;
        bool cachedMapFound;
        bool lastLoadAttemptFailed;*/
    };

    typedef QList<Info *> Infos;

public:
    MapArchive();

    /**
     * To be called to register the cvars and ccmds for this module.
     */
    static void consoleRegister();

    /**
     * Clear the map archive, removing all existing map information.
     */
    void clear();

    /// Convenient alias for clear()
    inline void reset() { clear(); }

    /**
     * Attempt to locate the info for a map in the archive by URI.
     *
     * @param uri  Map identifier.
     *
     * @return  Pointer to the found info; otherwise @c 0.
     */
    Info *findInfo(Uri const &uri) const;

    /**
     * Create a new info for a map in the archive. If existing info is
     * found it will be returned instead (becomes a no-op).
     *
     * @param uri  Map identifier.
     *
     * @return  Possibly newly-created Info for the map.
     */
    Info &createInfo(Uri const &uri);

    /**
     * Attempt to load the map associated with the specified identifier.
     * Intended as a convenient shorthand and equivalent to the calltree:
     *
     * @code
     *   createInfo(@a uri).loadMap();
     * @endcode
     *
     * @return  Pointer to the loaded GameMap; otherwise @c 0.
     */
    inline GameMap *loadMap(Uri const &uri)
    {
        // Record this map if we haven't already and load then it in!
        return createInfo(uri).loadMap();
    }

    /**
     * Provides access to the archive's map info for efficient traversal.
     */
    Infos const &infos() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG_ARCHIVED_MAP_MAIN_H
