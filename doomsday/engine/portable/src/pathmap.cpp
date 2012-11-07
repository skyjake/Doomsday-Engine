/**
 * @file pathmap.cpp
 *
 * Fragment map of a delimited string @ingroup fs
 *
 * @author Copyright &copy; 2011-2012 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"
#include "pathmap.h"
#include "pathtree.h"

#include <ctype.h>
#include <de/memory.h>

static ushort PathMap_HashFragment(PathMap& pm, PathMapFragment& fragment)
{
    // Is it time to compute the hash for this fragment?
    if(fragment.hash == PATHTREE_NOHASH)
    {
        size_t fragmentLength = (fragment.to - fragment.from) + 1;
        fragment.hash = pm.hashFragmentFunc(fragment.from, fragmentLength, pm.delimiter);
    }
    return fragment.hash;
}

/// @return  @c true iff @a fragment comes from the static fragment buffer
///          allocated along with @a pm.
static inline bool PathMap_IsStaticFragment(PathMap const& pm, PathMapFragment const& fragment)
{
    return &fragment >= pm.fragmentBuffer &&
           &fragment < (pm.fragmentBuffer + sizeof(*pm.fragmentBuffer) * PATHMAP_FRAGMENTBUFFER_SIZE);
}

static PathMapFragment* PathMap_AllocFragment(PathMap& pm)
{
    // Retrieve another fragment.
    PathMapFragment* fragment;
    if(pm.fragmentCount < PATHMAP_FRAGMENTBUFFER_SIZE)
    {
        fragment = pm.fragmentBuffer + pm.fragmentCount;
    }
    else
    {
        // Allocate an "extra" fragment.
        fragment = (PathMapFragment*) M_Malloc(sizeof *fragment);
        if(!fragment) Con_Error("PathMap::AllocFragment: Failed on allocation of %lu bytes for new PathMap::Fragment.", (unsigned long) sizeof *fragment);
    }

    fragment->from = fragment->to = NULL;

    // Hashing is deferred; means not-hashed yet.
    fragment->hash = PATHTREE_NOHASH;
    fragment->parent = NULL;

    // There is now one more fragment in the map.
    pm.fragmentCount += 1;

    return fragment;
}

static void PathMap_MapAllFragments(PathMap& pm, char const* path, size_t pathLen)
{
    PathMapFragment* fragment = NULL;
    char const* begin = path;
    char const* to = begin + pathLen - 1;
    char const* from;

    pm.fragmentCount = 0;
    pm.extraFragments = NULL;

    if(pathLen == 0) return;

    // Skip over any trailing delimiters.
    for(size_t i = pathLen; *to && *to == pm.delimiter && i-- > 0; to--)
    {}

    // Scan for discreet fragments in the path, in reverse order.
    forever
    {
        // Find the start of the next path fragment.
        for(from = to; from > begin && !(*from == pm.delimiter); from--)
        {}

        PathMapFragment* newFragment = PathMap_AllocFragment(pm);

        // If this is an "extra" fragment, link it to the tail of the list.
        if(!PathMap_IsStaticFragment(pm, *newFragment))
        {
            if(!pm.extraFragments)
            {
                pm.extraFragments = newFragment;
            }
            else
            {
                fragment->parent = pm.extraFragments;
            }
        }

        fragment = newFragment;

        fragment->from = (*from == pm.delimiter? from + 1 : from);
        fragment->to   = to;

        // Are there no more parent directories?
        if(from == begin) break;

        // So far so good. Move one directory level upwards.
        // The next fragment ends here.
        to = from-1;
    }

    // Deal with the special case of a Unix style zero-length root name.
    if(*begin == pm.delimiter)
    {
        fragment = PathMap_AllocFragment(pm);
        fragment->from = fragment->to = "";
    }
}

static void PathMap_ClearFragments(PathMap& pm)
{
    while(pm.extraFragments)
    {
        PathMapFragment* next = pm.extraFragments->parent;
        M_Free(pm.extraFragments);
        pm.extraFragments = next;
    }
}

pathmap_s::pathmap_s(hashpathfragmentfunc_t hashFragmentFunc, char const* path, char delimiter)
{
    PathMap_Initialize2(this, hashFragmentFunc, path, delimiter);
}

pathmap_s::~pathmap_s()
{
    PathMap_Destroy(this);
}

PathMap* PathMap_Initialize2(PathMap* pm,
    hashpathfragmentfunc_t hashFragmentFunc, char const* path, char delimiter)
{
    DENG_ASSERT(pm && hashFragmentFunc);

#if _DEBUG
    // Perform unit tests.
    PathMap_Test();
#endif

    pm->path = path;
    pm->delimiter = delimiter;
    pm->hashFragmentFunc = hashFragmentFunc;

    // Create the fragment map of the path.
    PathMap_MapAllFragments(*pm, pm->path, pm->path? strlen(pm->path) : 0);

    // Hash the first (i.e., rightmost) fragment right away.
    PathMap_Fragment(pm, 0);

    return pm;
}

PathMap* PathMap_Initialize(PathMap* pm,
    hashpathfragmentfunc_t hashFragmentCallback, char const* path)
{
    return PathMap_Initialize2(pm, hashFragmentCallback, path, '/');
}

void PathMap_Destroy(PathMap* pm)
{
    DENG_ASSERT(pm);
    PathMap_ClearFragments(*pm);
}

uint PathMap_Size(PathMap* pm)
{
    DENG_ASSERT(pm);
    return pm->fragmentCount;
}

PathMapFragment* PathMap_Fragment(PathMap* pm, uint idx)
{
    DENG_ASSERT(pm);

    if(idx >= pm->fragmentCount) return NULL;

    PathMapFragment* fragment;
    if(idx < PATHMAP_FRAGMENTBUFFER_SIZE)
    {
        fragment = pm->fragmentBuffer + idx;
    }
    else
    {
        uint n = PATHMAP_FRAGMENTBUFFER_SIZE;
        fragment = pm->extraFragments;
        while(n++ < idx)
        {
            fragment = fragment->parent;
        }
    }
    PathMap_HashFragment(*pm, *fragment);
    return fragment;
}

#if _DEBUG
static ushort hashPathFragment(char const* fragment, size_t len, char delimiter)
{
    DENG2_ASSERT(fragment);

    // Skip over any trailing delimiters.
    char const* c = fragment + len - 1;
    while(c >= fragment && *c && *c == delimiter)
    { c--; }

    // Compose the hash.
    int op = 0;
    ushort key = 0;
    for(; c >= fragment && *c && *c != delimiter; c--)
    {
        switch(op)
        {
        case 0: key ^= tolower(*c); ++op;   break;
        case 1: key *= tolower(*c); ++op;   break;
        case 2: key -= tolower(*c);   op=0; break;
        }
    }
    return key % 512 /* Number of buckets */;
}

void PathMap_Test(void)
{
    static bool alreadyTested = false;

    PathMapFragment const* fragment;
    size_t len;

    if(alreadyTested) return;
    alreadyTested = true;

    // Test a zero-length path.
    {
    PathMap pm = PathMap(hashPathFragment, "");
    DENG_ASSERT(PathMap_Size(&pm) == 0);
    }

    // Test a Windows style path with a drive plus file path.
    {
    PathMap pm = PathMap(hashPathFragment, "c:/something.ext");
    DENG_ASSERT(PathMap_Size(&pm) == 2);

    fragment = PathMap_Fragment(&pm, 0);
    len = fragment->to - fragment->from;
    DENG_ASSERT(len == 12);
    DENG_ASSERT(!strncmp(fragment->from, "something.ext", len+1));

    fragment = PathMap_Fragment(&pm, 1);
    len = fragment->to - fragment->from;
    DENG_ASSERT(len == 1);
    DENG_ASSERT(!strncmp(fragment->from, "c:", len+1));
    }

    // Test a Unix style path with a zero-length root node name.
    {
    PathMap pm = PathMap(hashPathFragment, "/something.ext");
    DENG_ASSERT(PathMap_Size(&pm) == 2);

    fragment = PathMap_Fragment(&pm, 0);
    len = fragment->to - fragment->from;
    DENG_ASSERT(len == 12);
    DENG_ASSERT(!strncmp(fragment->from, "something.ext", len+1));

    fragment = PathMap_Fragment(&pm, 1);
    len = fragment->to - fragment->from;
    DENG_ASSERT(len == 0);
    DENG_ASSERT(!strncmp(fragment->from, "", len));
    }

    // Test a relative directory.
    {
    PathMap pm = PathMap(hashPathFragment, "some/dir/structure/");
    DENG_ASSERT(PathMap_Size(&pm) == 3);

    fragment = PathMap_Fragment(&pm, 0);
    len = fragment->to - fragment->from;
    DENG_ASSERT(len == 8);
    DENG_ASSERT(!strncmp(fragment->from, "structure", len+1));

    fragment = PathMap_Fragment(&pm, 1);
    len = fragment->to - fragment->from;
    DENG_ASSERT(len == 2);
    DENG_ASSERT(!strncmp(fragment->from, "dir", len+1));

    fragment = PathMap_Fragment(&pm, 2);
    len = fragment->to - fragment->from;
    DENG_ASSERT(len == 3);
    DENG_ASSERT(!strncmp(fragment->from, "some", len+1));
    }
}
#endif
