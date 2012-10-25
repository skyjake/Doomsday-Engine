/**
 * @file pathmap.h
 * Fragment map of a delimited string. @ingroup fs
 *
 * @authors Copyright © 2011-2012 Daniel Swanson <danij@dengine.net>
 *
 * A map of a fragment-delimited string. Instantiate on the stack.
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

#ifndef LIBDENG_FILESYS_PATHMAP_H
#define LIBDENG_FILESYS_PATHMAP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Path fragment info.
 */
typedef struct pathmapfragment_s {
    ushort hash;
    const char* from, *to;
    struct pathmapfragment_s* next;

#ifdef __cplusplus
    // Determine length of fragment in characters.
    int length() const
    {
        if(!qstrcmp(to, "") && !qstrcmp(from, ""))
            return 0;
        return (to - from) + 1;
    }
#endif
} PathMapFragment;

/// Size of the fixed-length "short" fragment buffer allocated with the map.
#define PATHMAP_FRAGMENTBUFFER_SIZE 24

/**
 * Callback function type for path fragment hashing algorithms.
 *
 * @param path          Path fragment to be hashed.
 * @param len           Length of @a path in characters (excluding terminator).
 * @param delimiter     Fragments in @a path are delimited by this character.
 *
 * @return The resultant hash key.
 */
typedef ushort (*hashpathfragmentcallback_t)(const char* path, size_t len, char delimiter);

typedef struct pathmap_s {
    const char* path; ///< The mapped path.
    char delimiter; ///< Character used to delimit path fragments.
    uint fragmentCount; ///< Total number of fragments in the path.

    /**
     * Fragment map of the path. The map is composed of two
     * components; the first PATHMAP_FRAGMENTBUFFER_SIZE elements
     * are placed into a fixed-size buffer allocated along with
     * the map. Any additional fragments are attached to "this" using
     * a linked list of nodes.
     *
     * This optimized representation hopefully means that the
     * majority of paths can be represented without dynamically
     * allocating memory from the heap.
     */
    PathMapFragment fragmentBuffer[PATHMAP_FRAGMENTBUFFER_SIZE];

    ///< Head of the linked list of "extra" fragments, in reverse order.
    PathMapFragment* extraFragments;

    /// Path fragment hashing callback.
    hashpathfragmentcallback_t hashFragmentCallback;
} PathMap;

/**
 * Initialize the specified PathMap from @a path.
 *
 * @post The path will have been subdivided into a fragment map
 * and some or all of the fragment hashes will have been calculated
 * (dependant on the number of discreet fragments).
 *
 * @param path          Relative or absolute path to be mapped. Assumed to remain
 *                      accessible until PathMap_Destroy() is called.
 *
 * @param hashFragmentCallback  Path fragment hashing algorithm callback.
 *
 * @param delimiter     Fragments of @a path are delimited by this character.
 *
 * @return  Pointer to "this" instance for caller convenience.
 */
PathMap* PathMap_Initialize2(PathMap* pathMap,
    hashpathfragmentcallback_t hashFragmentCallback, const char* path, char delimiter);
PathMap* PathMap_Initialize(PathMap* pathMap,
    hashpathfragmentcallback_t hashFragmentCallback, const char* path); /*delimiter='/'*/

/**
 * Destroy @a pathMap releasing any resources acquired for it.
 */
void PathMap_Destroy(PathMap* pathMap);

/// @return  Number of fragments in the mapped path.
uint PathMap_Size(PathMap* pathMap);

/**
 * Retrieve the info for fragment @a idx within the path. Note that
 * fragments are indexed in reverse order (compared to the logical,
 * left-to-right order of the original path).
 *
 * For example, if the mapped path is "c:/mystuff/myaddon.addon"
 * the corresponding fragment map will be arranged as follows:
 * <pre>
 *   [0:{myaddon.addon}, 1:{mystuff}, 2:{c:}].
 * </pre>
 *
 * @post Hash may have been calculated for the referenced fragment.
 *
 * @param idx  Reverse-index of the fragment to be retrieved.
 *
 * @return  Processed fragment info else @c NULL if @a idx is invalid.
 */
const PathMapFragment* PathMap_Fragment(PathMap* pathMap, uint idx);

#if _DEBUG
/**
 * Perform unit tests for this class.
 */
void PathMap_Test(void);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_FILESYS_PATHMAP_H
