/**\file pathmap.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2011-2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "de_base.h"
#include "de_console.h"

#include "pathmap.h"

static ushort PathMap_HashFragment(PathMap* pm, PathMapFragment* fragment)
{
    assert(pm && fragment);
    // Is it time to compute the hash for this fragment?
    if(fragment->hash == PATHDIRECTORY_NOHASH)
    {
        fragment->hash = PathDirectory_HashPath(fragment->from,
            (fragment->to - fragment->from) + 1, pm->_delimiter);
    }
    return fragment->hash;
}

static void PathMap_MapAllFragments(PathMap* pm, const char* path, size_t pathLen)
{
    PathMapFragment* fragment = NULL;
    const char* begin = path;
    const char* to = begin + pathLen - 1;
    const char* from;
    size_t i;
    assert(pm && path && path[0] && pathLen != 0);

    // Skip over any trailing delimiters.
    for(i = pathLen; *to && *to == pm->_delimiter && i-- > 0; to--) {}

    pm->_fragmentCount = 0;
    pm->_extraFragments = NULL;

    // Scan for discreet fragments in the path, in reverse order.
    for(;;)
    {
        // Find the start of the next path fragment.
        for(from = to; from > begin && !(*from == pm->_delimiter); from--) {}

        // Retrieve another fragment.
        if(pm->_fragmentCount < PATHMAP_FRAGMENTBUFFER_SIZE)
        {
            fragment = pm->_fragmentBuffer + pm->_fragmentCount;
        }
        else
        {
            // Allocate another "extra" fragment.
            PathMapFragment* f = (PathMapFragment*)malloc(sizeof *f);
            if(!f) Con_Error("PathMap::MapAllFragments: Failed on allocation of %lu bytes for new PathMap::Fragment.", (unsigned long) sizeof *f);

            if(!pm->_extraFragments)
            {
                pm->_extraFragments = fragment = f;
            }
            else
            {
                fragment->next = f;
                fragment = f;
            }
        }

        fragment->from = (*from == pm->_delimiter? from + 1 : from);
        fragment->to   = to;
        // Hashing is deferred; means not-hashed yet.
        fragment->hash = PATHDIRECTORY_NOHASH;
        fragment->next = NULL;

        // There is now one more fragment in the map.
        pm->_fragmentCount += 1;

        // Are there no more parent directories?
        if(from == begin) break;

        // So far so good. Move one directory level upwards.
        // The next fragment ends here.
        to = from-1;
    }
}

static void PathMap_ClearFragments(PathMap* pm)
{
    assert(pm);
    while(pm->_extraFragments)
    {
        PathMapFragment* next = pm->_extraFragments->next;
        free(pm->_extraFragments);
        pm->_extraFragments = next;
    }
}

static void PathMap_ClearPath(PathMap* pm)
{
    assert(pm);
    if(pm->_path) free(pm->_path), pm->_path = NULL;
}

PathMap* PathMap_Initialize2(PathMap* pm, const char* path, char delimiter)
{
    char* pathBuffer;
    size_t pathLen;
    assert(pm);

     // Take a copy of the search term.
    pathLen = (path? strlen(path) : 0);
    if(pathLen <= PATHMAP_SHORT_PATH)
    {
        // Use the small search path buffer.
        pm->_path = NULL;
        pathBuffer = pm->_shortPath;
    }
    else
    {
        // Allocate a buffer large enough to hold the whole path.
        pm->_path = (char*)malloc(pathLen+1);
        pathBuffer = pm->_path;
    }
    if(pathLen)
    {
        memcpy(pathBuffer, path, pathLen);
    }
    pathBuffer[pathLen] = '\0';

    pm->_delimiter = delimiter;

    // Create the fragment map of the path.
    PathMap_MapAllFragments(pm, pathBuffer, pathLen);

    // Hash the first (i.e., rightmost) fragment right away.
    PathMap_Fragment(pm, 0);

    return pm;
}

PathMap* PathMap_Initialize(PathMap* pm, const char* path)
{
    return PathMap_Initialize2(pm, path, '/');
}

void PathMap_Destroy(PathMap* pm)
{
    assert(pm);
    PathMap_ClearFragments(pm);
    PathMap_ClearPath(pm);
}

uint PathMap_Size(PathMap* pm)
{
    assert(pm);
    return pm->_fragmentCount;
}

const PathMapFragment* PathMap_Fragment(PathMap* pm, uint idx)
{
    PathMapFragment* fragment;
    if(!pm || idx >= pm->_fragmentCount) return NULL;
    if(idx < PATHMAP_FRAGMENTBUFFER_SIZE)
    {
        fragment = pm->_fragmentBuffer + idx;
    }
    else
    {
        uint n = PATHMAP_FRAGMENTBUFFER_SIZE;
        fragment = pm->_extraFragments;
        while(n++ < idx)
        {
            fragment = fragment->next;
        }
    }
    PathMap_HashFragment(pm, fragment);
    return fragment;
}
