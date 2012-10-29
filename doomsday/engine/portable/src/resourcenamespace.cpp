/**
 * @file resourcenamespace.cpp
 *
 * Resource namespace. @ingroup resource
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h"
#include "de_console.h"
#include "de_filesys.h"

#include "pathtree.h"
#include "resourcenamespace.h"

namespace de {

struct ResourceNode
{
    /// Directory node for this resource in the owning PathTree
    PathTree::Node* directoryNode;

#if _DEBUG
    /// Symbolic name of this resource.
    ddstring_t name;
#endif

    ResourceNode(PathTree::Node& _directoryNode) : directoryNode(&_directoryNode) {
#if _DEBUG
        Str_Init(&name);
#endif
    }

    ~ResourceNode() {
#if _DEBUG
        Str_Free(&name);
#endif
    }

#if _DEBUG
    ResourceNode& setName(char const* newName) {
        Str_Set(&name, newName);
        return *this;
    }
#endif
};

/**
 * Name search hash.
 */
struct NameHash
{
    struct Node
    {
        Node* next;
        ResourceNode resource;

        Node(PathTree::Node& resourceNode)
            : next(0), resource(resourceNode)
        {}
    };

    struct Bucket
    {
        Node* first;
        Node* last;
    };
    Bucket buckets[RESOURCENAMESPACE_HASHSIZE];

    ~NameHash()
    {
        clear();
    }

    void clear()
    {
        for(uint i = 0; i < RESOURCENAMESPACE_HASHSIZE; ++i)
        {
            while(buckets[i].first)
            {
                NameHash::Node* nextNode = buckets[i].first->next;
                delete buckets[i].first;
                buckets[i].first = nextNode;
            }
            buckets[i].last = 0;
        }
    }
};

struct ResourceNamespace::Instance
{
    /// Resource name hashing callback.
    ResourceNamespace::HashNameFunc* hashNameFunc;

    /// Name hash table.
    NameHash nameHash;

    /// Sets of search paths known by this namespace.
    /// Each set is in order of greatest-importance, right to left.
    ResourceNamespace::SearchPaths searchPaths;

    Instance(ResourceNamespace::HashNameFunc* _hashNameFunc)
        : hashNameFunc(_hashNameFunc)
    {}

    NameHash::Node* findResourceInNameHash(ResourceNamespace::NameHashKey key,
        PathTree::Node const& ptNode)
    {
        NameHash::Node* node = nameHash.buckets[key].first;
        while(node && node->resource.directoryNode != &ptNode)
        {
            node = node->next;
        }
        return node;
    }
};

ResourceNamespace::ResourceNamespace(ResourceNamespace::HashNameFunc* hashNameFunc)
{
    DENG_ASSERT(hashNameFunc);
    d = new Instance(hashNameFunc);
}

ResourceNamespace::~ResourceNamespace()
{
    delete d;
}

void ResourceNamespace::clear()
{
    d->nameHash.clear();
}

bool ResourceNamespace::add(ddstring_t const* name, PathTree::Node& resourceNode)
{
    DENG_ASSERT(name);

    NameHashKey key = d->hashNameFunc(name);
    if(key >= RESOURCENAMESPACE_HASHSIZE)
    {
        Con_Error("ResourceNamespace::Add: Hashing of name '%s' in [%p] produced invalid key %u.", Str_Text(name), (void*)this, (unsigned short)key);
        exit(1); // Unreachable.
    }

    // Is this a new resource?
    bool isNewNode = false;
    NameHash::Node* hashNode = d->findResourceInNameHash(key, resourceNode);
    if(!hashNode)
    {
        isNewNode = true;

        // Create a new hash node for this.
        hashNode = new NameHash::Node(resourceNode);
#if _DEBUG
        // Take a copy of the name to aid in tracing bugs, etc...
        hashNode->resource.setName(Str_Text(name));
#endif

        // Link it to the list for this bucket.
        NameHash::Bucket& bucket = d->nameHash.buckets[key];
        if(bucket.last) bucket.last->next = hashNode;
        bucket.last = hashNode;
        if(!bucket.first) bucket.first = hashNode;
    }

    // (Re)configure this record.
    ResourceNode* res = &hashNode->resource;
    res->directoryNode = &resourceNode;

    return isNewNode;
}

ResourceNamespace::SearchPaths const& ResourceNamespace::searchPaths() const
{
    return d->searchPaths;
}

bool ResourceNamespace::addSearchPath(PathGroup group, Uri const* _searchPath, int flags)
{
    // Is this suitable?
    if(!_searchPath || Str_IsEmpty(Uri_Path(_searchPath)) ||
       !Str_CompareIgnoreCase(Uri_Path(_searchPath), "/"))
        return false;

    // Ensure this is a well formed path (only directories can be search paths).
    AutoStr* path = Uri_Compose(_searchPath);
    F_AppendMissingSlash(path);

    Uri* searchPath = Uri_NewWithPath2(Str_Text(path), RC_NULL);

    // Have we seen this path already (we don't want duplicates)?
    DENG2_FOR_EACH(SearchPaths, i, d->searchPaths)
    {
        if(Uri_Equality(i->uri, searchPath))
        {
            i->flags = flags;
            Uri_Delete(searchPath);
            return true;
        }
    }

    // Prepend to the path list - newer paths have priority.
    d->searchPaths.insert(group, SearchPath(flags, searchPath));

    return true;
}

void ResourceNamespace::clearSearchPaths(PathGroup group)
{
    d->searchPaths.remove(group);
}

void ResourceNamespace::clearSearchPaths()
{
    d->searchPaths.clear();
}

ddstring_t* ResourceNamespace::listSearchPaths(PathGroup group, ddstring_t* pathList,
    char delimiter) const
{
    if(pathList)
    {
        ResourceNamespace::SearchPaths::const_iterator i = d->searchPaths.find(group);
        while(i != d->searchPaths.end() && i.key() == group)
        {
            AutoStr* path = Uri_Compose(i.value().uri);
            Str_Appendf(pathList, "%s%c", Str_Text(path), delimiter);
        }
    }
    return pathList;
}

#if _DEBUG
void ResourceNamespace::debugPrint() const
{
    Con_Printf("ResourceNamespace [%p]:\n", (void*)this);
    uint resIdx = 0;
    for(uint key = 0; key < RESOURCENAMESPACE_HASHSIZE; ++key)
    {
        NameHash::Bucket& bucket = d->nameHash.buckets[key];
        for(NameHash::Node* node = bucket.first; node; node = node->next)
        {
            Con_Printf("  %lu: %lu:", (unsigned long)resIdx, (unsigned long)key);

            ResourceNode const& res = node->resource;
            AutoStr* path = res.directoryNode->composePath(AutoStr_NewStd(), NULL, DIR_SEP_CHAR);
            Con_Printf("\"%s\" -> %s\n", Str_Text(&res.name), Str_Text(path));

            ++resIdx;
        }
    }
    Con_Printf("  %lu %s in namespace.\n", (unsigned long) resIdx, (resIdx == 1? "resource" : "resources"));
}
#endif

int ResourceNamespace::findAll(ddstring_t const* name, ResourceList& found)
{
    int numFoundSoFar = found.count();

    NameHashKey from, to;
    if(name)
    {
        from = d->hashNameFunc(name);
        if(from >= RESOURCENAMESPACE_HASHSIZE)
        {
            Con_Error("ResourceNamespace::Iterate: Hashing of name '%s' in [%p] produced invalid key %u.", Str_Text(name), (void*)this, (unsigned short)from);
            exit(1); // Unreachable.
        }
        to = from;
    }
    else
    {
        from = 0;
        to = RESOURCENAMESPACE_HASHSIZE - 1;
    }

    for(NameHashKey key = from; key < to + 1; ++key)
    {
        NameHash::Bucket& bucket = d->nameHash.buckets[key];
        for(NameHash::Node* hashNode = bucket.first; hashNode; hashNode = hashNode->next)
        {
            ResourceNode& resource = hashNode->resource;
            if(Str_CompareIgnoreCase(resource.directoryNode->name(), Str_Text(name))) continue;

            found.push_back(resource.directoryNode);
        }
    }

    return found.count() - numFoundSoFar;
}

} // namespace de
