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

#include <cctype>

#include "de_filesys.h"

#include <de/Log>
#include "filedirectory.h"
#include "pathtree.h"

#include "resourcenamespace.h"

extern "C" filename_t ddBasePath;

namespace de {

class Resource
{
public:
    Resource(PathTree::Node& _directoryNode) : directoryNode_(&_directoryNode) {
#if _DEBUG
        Str_Init(&name_);
#endif
    }

    ~Resource() {
#if _DEBUG
        Str_Free(&name_);
#endif
    }

    PathTree::Node& directoryNode() const {
        return *directoryNode_;
    }

    Resource& setDirectoryNode(PathTree::Node& newDirectoryNode) {
        directoryNode_ = &newDirectoryNode;
        return *this;
    }

#if _DEBUG
    ddstring_t const& name() const {
        return name_;
    }

    Resource& setName(char const* newName) {
        Str_Set(&name_, newName);
        return *this;
    }
#endif

private:
    /// Directory node for this resource in the owning PathTree
    PathTree::Node* directoryNode_;

#if _DEBUG
    /// Symbolic name of this resource.
    ddstring_t name_;
#endif
};

/**
 * Name search hash.
 */
struct NameHash
{
public:
    /// Type used to represent hash keys.
    typedef unsigned short key_type;

    /// Number of buckets in the hash table.
    static unsigned short const hash_size = 512;

    struct Node
    {
        Node* next;
        Resource resource;

        Node(PathTree::Node& resourceNode)
            : next(0), resource(resourceNode)
        {}
    };

    struct Bucket
    {
        Node* first;
        Node* last;
    };

public:
    Bucket buckets[hash_size];

public:
    NameHash()
    {
        memset(buckets, 0, sizeof(buckets));
    }

    ~NameHash()
    {
        clear();
    }

    void clear()
    {
        for(uint i = 0; i < hash_size; ++i)
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

//unsigned short const NameHash::hash_size;

static AutoStr* composeResourceName(ddstring_t const* filePath)
{
    AutoStr* name = AutoStr_NewStd();
    F_FileName(name, Str_Text(filePath));
    return name;
}

/**
 * This is a hash function. It uses the resource name to generate a
 * somewhat-random number between 0 and NameHash::hash_size.
 *
 * @return  The generated hash key.
 */
static NameHash::key_type hashResourceName(ddstring_t const* name)
{
    DENG_ASSERT(name);

    NameHash::key_type key = 0;
    byte op = 0;

    for(char const* c = Str_Text(name); *c; c++)
    {
        switch(op)
        {
        case 0: key ^= tolower(*c); ++op;   break;
        case 1: key *= tolower(*c); ++op;   break;
        case 2: key -= tolower(*c);   op=0; break;
        }
    }
    return key % NameHash::hash_size;
}

static int addResourceWorker(PathTree::Node& node, void* parameters)
{
    // We are only interested in leafs (i.e., files and not directories).
    if(node.type() == PathTree::Leaf)
    {
        ResourceNamespace& rnamespace = *((ResourceNamespace*) parameters);
        rnamespace.add(node);
    }
    return 0; // Continue adding.
}

#define RNF_IS_DIRTY            0x80 // Filehash needs to be (re)built (avoid allocating an empty name hash).

struct ResourceNamespace::Instance
{
    ResourceNamespace& self;

    /// Associated path directory for this namespace.
    /// @todo It should not be necessary for a unique directory per namespace.
    FileDirectory* directory;

    byte flags;

    /// Name hash table.
    NameHash nameHash;

    /// Sets of search paths known by this namespace.
    /// Each set is in order of greatest-importance, right to left.
    ResourceNamespace::SearchPaths searchPaths;

    /// Symbolic name of this namespace.
    ddstring_t name;

    Instance(ResourceNamespace& d, char const* _name)
        : self(d), directory(new FileDirectory(ddBasePath)), flags(RNF_IS_DIRTY)
    {
        Str_InitStd(&name);
        if(_name) Str_Set(&name, _name);
    }

    ~Instance()
    {
        Str_Free(&name);
        delete directory;
    }

    NameHash::Node* findResourceInNameHash(NameHash::key_type key,
        PathTree::Node const& ptNode)
    {
        NameHash::Node* node = nameHash.buckets[key].first;
        while(node && &node->resource.directoryNode() != &ptNode)
        {
            node = node->next;
        }
        return node;
    }

    void addFromSearchPaths(ResourceNamespace::PathGroup group)
    {
        for(ResourceNamespace::SearchPaths::const_iterator i = searchPaths.find(group);
            i != searchPaths.end() && i.key() == group; ++i)
        {
            ResourceNamespace::SearchPath const& searchPath = *i;
            directory->addPath(searchPath.flags(), searchPath.uri(), addResourceWorker, (void*)&self);
        }
    }
};

ResourceNamespace::ResourceNamespace(char const* symbolicName)
{
    d = new Instance(*this, symbolicName);
}

ResourceNamespace::~ResourceNamespace()
{
    delete d;
}

ddstring_t const* ResourceNamespace::name() const
{
    return &d->name;
}

void ResourceNamespace::clear()
{
    d->nameHash.clear();
    d->directory->clear();
    d->flags |= RNF_IS_DIRTY;
}

void ResourceNamespace::rebuild()
{
    // Is a rebuild necessary?
    if(!(d->flags & RNF_IS_DIRTY)) return;

/*#if _DEBUG
    uint startTime;
    VERBOSE( Con_Message("Rebuilding ResourceNamespace '%s'...\n", Str_Text(d->name)) )
    VERBOSE2( startTime = Sys_GetRealTime() )
    VERBOSE2( Con_PrintPathList(Str_Text(listSearchPaths(AutoStr_NewStd()))) )
#endif*/

    // (Re)populate the directory and add found resources.
    clear();
    d->addFromSearchPaths(ResourceNamespace::OverridePaths);
    d->addFromSearchPaths(ResourceNamespace::ExtraPaths);
    d->addFromSearchPaths(ResourceNamespace::DefaultPaths);
    d->addFromSearchPaths(ResourceNamespace::FallbackPaths);

    d->flags &= ~RNF_IS_DIRTY;

/*#if _DEBUG
    VERBOSE2( FileDirectory::debugPrint(d->directory) )
    VERBOSE2( debugPrint() )
    VERBOSE2( Con_Message("  Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) )
#endif*/
}

bool ResourceNamespace::add(PathTree::Node& resourceNode)
{
    AutoStr* name = composeResourceName(resourceNode.name());
    NameHash::key_type key = hashResourceName(name);

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

        // We will need to rebuild this namespace (if we aren't already doing so,
        // in the case of auto-populated namespaces built from FileDirectorys).
        d->flags |= RNF_IS_DIRTY;
    }

    // (Re)configure this record.
    Resource* res = &hashNode->resource;
    res->setDirectoryNode(resourceNode);

    return isNewNode;
}

bool ResourceNamespace::addSearchPath(PathGroup group, Uri const* _searchPath, int flags)
{
    // Ensure this is a well formed path.
    if(!_searchPath || Str_IsEmpty(Uri_Path(_searchPath)) ||
       !Str_CompareIgnoreCase(Uri_Path(_searchPath), "/") ||
       Str_RAt(Uri_Path(_searchPath), 0) != '/')
        return false;

    // The addition of a new search path means the namespace is now dirty.
    d->flags |= RNF_IS_DIRTY;

    // Have we seen this path already (we don't want duplicates)?
    DENG2_FOR_EACH(SearchPaths, i, d->searchPaths)
    {
        if(Uri_Equality(i->uri(), _searchPath))
        {
            i->setFlags(flags);
            return true;
        }
    }

    // Prepend to the path list - newer paths have priority.
    d->searchPaths.insert(group, SearchPath(flags, Uri_NewCopy(_searchPath)));

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
        for(ResourceNamespace::SearchPaths::const_iterator i = d->searchPaths.find(group);
            i != d->searchPaths.end() && i.key() == group; ++i)
        {
            AutoStr* path = Uri_Compose(i.value().uri());
            Str_Appendf(pathList, "%s%c", Str_Text(path), delimiter);
        }
    }
    return pathList;
}

ResourceNamespace::SearchPaths const& ResourceNamespace::searchPaths() const
{
    return d->searchPaths;
}

#if _DEBUG
void ResourceNamespace::debugPrint() const
{
    LOG_AS("ResourceNamespace::debugPrint");
    LOG_DEBUG("[%p]:") << de::dintptr(this);

    uint resIdx = 0;
    for(NameHash::key_type key = 0; key < NameHash::hash_size; ++key)
    {
        NameHash::Bucket& bucket = d->nameHash.buckets[key];
        for(NameHash::Node* node = bucket.first; node; node = node->next)
        {
            Resource const& res = node->resource;
            AutoStr* path = res.directoryNode().composePath(AutoStr_NewStd(), NULL);

            LOG_DEBUG("  %u - %u:\"%s\" => %s")
                    << resIdx << key << Str_Text(&res.name()) << Str_Text(path);

            ++resIdx;
        }
    }

    LOG_DEBUG("  %lu %s in namespace.") << resIdx << (resIdx == 1? "resource" : "resources");
}
#endif

int ResourceNamespace::findAll(ddstring_t const* searchPath, ResourceList& found)
{
    int numFoundSoFar = found.count();

    AutoStr* name = 0;
    if(searchPath && !Str_IsEmpty(searchPath))
    {
        name = composeResourceName(searchPath);
    }

    NameHash::key_type from, to;
    if(name)
    {
        from = hashResourceName(name);
        to   = from;
    }
    else
    {
        from = 0;
        to   = NameHash::hash_size - 1;
    }

    for(NameHash::key_type key = from; key < to + 1; ++key)
    {
        NameHash::Bucket& bucket = d->nameHash.buckets[key];
        for(NameHash::Node* hashNode = bucket.first; hashNode; hashNode = hashNode->next)
        {
            Resource& resource = hashNode->resource;
            if(name && qstrnicmp(Str_Text(name), Str_Text(resource.directoryNode().name()), Str_Length(name))) continue;

            found.push_back(&resource.directoryNode());
        }
    }

    return found.count() - numFoundSoFar;
}

ResourceNamespace::SearchPath::SearchPath(int _flags, Uri* _uri)
    : flags_(_flags), uri_(_uri)
{}

ResourceNamespace::SearchPath::SearchPath(SearchPath const& other)
    : flags_(other.flags_), uri_(Uri_NewCopy(other.uri_))
{}

ResourceNamespace::SearchPath::~SearchPath()
{
    Uri_Delete(uri_);
}

ResourceNamespace::SearchPath& ResourceNamespace::SearchPath::operator = (SearchPath other)
{
    swap(*this, other);
    return *this;
}

int ResourceNamespace::SearchPath::flags() const
{
    return flags_;
}

ResourceNamespace::SearchPath& ResourceNamespace::SearchPath::setFlags(int newFlags)
{
    flags_ = newFlags;
    return *this;
}

Uri const* ResourceNamespace::SearchPath::uri() const
{
    return uri_;
}

void swap(ResourceNamespace::SearchPath& first, ResourceNamespace::SearchPath& second)
{
    std::swap(first.flags_,  second.flags_);
    std::swap(first.uri_,    second.uri_);
}

} // namespace de
