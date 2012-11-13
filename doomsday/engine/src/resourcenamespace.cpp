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

#include "de_filesys.h"

#include <QDir>

#include <de/App>
#include <de/Log>
#include <de/NativePath>
#include "pathtree.h"

#include "resourcenamespace.h"

namespace de {

/**
 * Reference to a resource or file in the virtual file system.
 */
class ResourceRef
{
public:
    ResourceRef(PathTree::Node& _directoryNode)
        : directoryNode_(&_directoryNode)
    {}

    PathTree::Node& directoryNode() const {
        return *directoryNode_;
    }

    ResourceRef& setDirectoryNode(PathTree::Node& newDirectoryNode) {
        directoryNode_ = &newDirectoryNode;
        return *this;
    }

#if _DEBUG
    String const& name() const {
        return name_;
    }

    ResourceRef& setName(String const& newName) {
        name_ = newName;
        return *this;
    }
#endif

private:
    /// Directory node for this resource in the owning PathTree
    PathTree::Node* directoryNode_;

#if _DEBUG
    /// Symbolic name of this resource.
    String name_;
#endif
};

/**
 * Name search hash.
 */
struct NameHash
{
public:
    /// Type used to represent hash keys.
    typedef ushort hash_type;

    /// Number of buckets in the hash table.
    static hash_type const hash_range = 512;

    struct Node
    {
        Node* next;
        ResourceRef resource;

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
    Bucket buckets[hash_range];

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
        for(hash_type hashKey = 0; hashKey < hash_range; ++hashKey)
        {
            while(buckets[hashKey].first)
            {
                NameHash::Node* nextNode = buckets[hashKey].first->next;
                delete buckets[hashKey].first;
                buckets[hashKey].first = nextNode;
            }
            buckets[hashKey].last = 0;
        }
    }

    Node* findDirectoryNode(hash_type hashKey, PathTree::Node const& directoryNode)
    {
        Node* node = buckets[hashKey].first;
        while(node && &node->resource.directoryNode() != &directoryNode)
        {
            node = node->next;
        }
        return node;
    }

    static hash_type hashName(String const& str)
    {
        hash_type hashKey = 0;
        int op = 0;
        for(int i = 0; i < str.length(); ++i)
        {
            ushort unicode = str.at(i).toLower().unicode();
            switch(op)
            {
            case 0: hashKey ^= unicode; ++op;   break;
            case 1: hashKey *= unicode; ++op;   break;
            case 2: hashKey -= unicode;   op=0; break;
            }
        }
        return hashKey % hash_range;
    }
};

struct ResourceNamespace::Instance
{
    ResourceNamespace& self;

    /// Symbolic name of this namespace.
    String name;

    /// Associated path directory for this namespace.
    /// @todo It should not be necessary for a unique directory per namespace.
    PathTree directory;

    /// As the directory is relative, this special node servers as the root.
    PathTree::Node* rootNode;

    /// Name hash table.
    NameHash nameHash;

    /// Set to @c true when the name hash is obsolete/out-of-date and should be rebuilt.
    byte nameHashIsDirty;

    /// Sets of search paths known by this namespace.
    /// Each set is in order of greatest-importance, right to left.
    ResourceNamespace::SearchPaths searchPaths;

    Instance(ResourceNamespace& d, String _name)
        : self(d), name(_name), directory(), rootNode(0), nameHash(), nameHashIsDirty(true)
    {}

    /**
     * Add resources to this namespace by resolving search path, searching the
     * file system and populating our internal directory with the results.
     * Duplicates are automatically pruned.
     *
     * @param searchPath  The path to resolve and search.
     */
    void addFromSearchPath(ResourceNamespace::SearchPath const& searchPath)
    {
        try
        {
            // Add new nodes on this path and/or re-process previously seen nodes.
            addDirectoryPathNodesAndMaybeDescendBranch(true/*do descend*/, searchPath.uri().resolved(),
                                                       true/*is-directory*/, searchPath.flags());
        }
        catch(de::Uri::ResolveError const& er)
        {
            LOG_DEBUG(er.asText());
        }
    }

    /**
     * Add resources to this namespace by resolving each search path in @a group.
     *
     * @param group  Group of paths to search.
     */
    void addFromSearchPaths(ResourceNamespace::PathGroup group)
    {
        for(ResourceNamespace::SearchPaths::const_iterator i = searchPaths.find(group);
            i != searchPaths.end() && i.key() == group; ++i)
        {
            addFromSearchPath(*i);
        }
    }

    PathTree::Node* addDirectoryPathNodes(String path)
    {
        if(path.isEmpty()) return 0;

        // Try to make it a relative path.
        if(QDir::isAbsolutePath(path))
        {
            String basePath = QDir::fromNativeSeparators(DENG2_APP->nativeBasePath());
            if(path.beginsWith(basePath))
            {
                path = path.mid(basePath.length() + 1);
            }
        }

        // If this is equal to the base path, return that node.
        if(path.isEmpty())
        {
            // Time to construct the relative base node?
            if(!rootNode)
            {
                rootNode = directory.insert(Uri("./", RC_NULL));
            }
            return rootNode;
        }

        return directory.insert(Uri(path, RC_NULL));
    }

    void addDirectoryChildNodes(PathTree::Node& node, int flags)
    {
        if(node.isLeaf()) return;

        // Compose the search pattern. We're interested in *everything*.
        String searchPattern = node.composePath() / "*";

        // Process this search.
        FS1::PathList found;
        App_FileSystem()->findAllPaths(searchPattern, flags, found);
        DENG2_FOR_EACH_CONST(FS1::PathList, i, found)
        {
            addDirectoryPathNodesAndMaybeDescendBranch(!(flags & SPF_NO_DESCEND),
                                                       i->path, !!(i->attrib & A_SUBDIR),
                                                       flags);
        }
    }

    /**
     * @param descendBranch     @c true = descend this branch (if it is a branch).
     * @param filePath          Possibly-relative path to an element in the virtual file system.
     * param isFolder           @c true = @a filePath is a folder in the virtual file system.
     * @param flags             @ref searchPathFlags
     */
    void addDirectoryPathNodesAndMaybeDescendBranch(bool descendBranch,
        String filePath, bool /*isFolder*/, int flags)
    {
        // Add this path to the directory.
        PathTree::Node* node = addDirectoryPathNodes(filePath);
        if(!node) return;

        if(!node->isLeaf())
        {
            // Descend into this subdirectory?
            if(descendBranch)
            {
                // Already processed?
                if(node->userValue())
                {
                    // Process it again?
                    DENG2_FOR_EACH_CONST(PathTree::Nodes, i, directory.leafNodes())
                    {
                        PathTree::Node& sibling = **i;
                        if(sibling.parent() == node)
                        {
                            self.add(sibling);
                        }
                    }
                }
                else
                {
                    addDirectoryChildNodes(*node, flags);

                    // This node is now considered processed.
                    node->setUserValue(true);
                }
            }
        }
        // Node is a leaf.
        else
        {
            self.add(*node);

            // This node is now considered processed (if it wasn't already).
            node->setUserValue(true);
        }
    }
};

ResourceNamespace::ResourceNamespace(String symbolicName)
{
    d = new Instance(*this, symbolicName);
}

ResourceNamespace::~ResourceNamespace()
{
    delete d;
}

String const& ResourceNamespace::name() const
{
    return d->name;
}

void ResourceNamespace::clear()
{
    d->nameHash.clear();
    d->nameHashIsDirty = true;
    d->directory.clear();
    d->rootNode = 0;
}

void ResourceNamespace::rebuild()
{
    // Is a rebuild not necessary?
    if(!d->nameHashIsDirty) return;

    LOG_AS("ResourceNamespace::rebuild");
    LOG_DEBUG("Rebuilding '%s'...") << d->name;

    // uint startTime = Timer_RealMilliseconds();

    // (Re)populate the directory and add found resources.
    clear();
    d->addFromSearchPaths(ResourceNamespace::OverridePaths);
    d->addFromSearchPaths(ResourceNamespace::ExtraPaths);
    d->addFromSearchPaths(ResourceNamespace::DefaultPaths);
    d->addFromSearchPaths(ResourceNamespace::FallbackPaths);

    d->nameHashIsDirty = false;

    // LOG_INFO("Done in %.2f seconds.") << (Timer_RealMilliseconds() - startTime) / 1000.0f;

/*#if _DEBUG
    PathTree::debugPrint(d->directory);
    debugPrint();
#endif*/
}

static inline String composeResourceName(String const& filePath)
{
    return filePath.fileNameWithoutExtension();
}

bool ResourceNamespace::add(PathTree::Node& resourceNode)
{
    // We are only interested in leafs (i.e., files and not folders).
    if(!resourceNode.isLeaf()) return false;

    String name = composeResourceName(resourceNode.name());
    NameHash::hash_type hashKey = NameHash::hashName(name);

    // Is this a new resource?
    bool isNewNode = false;
    NameHash::Node* hashNode = d->nameHash.findDirectoryNode(hashKey, resourceNode);
    if(!hashNode)
    {
        isNewNode = true;

        // Create a new hash node for this.
        hashNode = new NameHash::Node(resourceNode);
#if _DEBUG
        // Take a copy of the name to aid in tracing bugs, etc...
        hashNode->resource.setName(name);
#endif

        // Link it to the list for this bucket.
        NameHash::Bucket& bucket = d->nameHash.buckets[hashKey];
        if(bucket.last) bucket.last->next = hashNode;
        bucket.last = hashNode;
        if(!bucket.first) bucket.first = hashNode;

        // We will need to rebuild this namespace (if we aren't already doing so,
        // in the case of auto-populated namespaces built from FileDirectorys).
        d->nameHashIsDirty = true;
    }

    // (Re)configure this record.
    ResourceRef* res = &hashNode->resource;
    res->setDirectoryNode(resourceNode);

    return isNewNode;
}

bool ResourceNamespace::addSearchPath(PathGroup group, de::Uri const& path, int flags)
{
    // Ensure this is a well formed path.
    if(path.isEmpty() ||
       !Str_CompareIgnoreCase(path.path(), "/") ||
       Str_RAt(path.path(), 0) != '/')
        return false;

    // The addition of a new search path means the namespace is now dirty.
    d->nameHashIsDirty = true;

    // Have we seen this path already (we don't want duplicates)?
    DENG2_FOR_EACH(SearchPaths, i, d->searchPaths)
    {
        // Compare using the unresolved textual representations.
        if(!i->uri().asText().compareWithoutCase(path.asText()))
        {
            i->setFlags(flags);
            return true;
        }
    }

    // Prepend to the path list - newer paths have priority.
    de::Uri* uriCopy = new de::Uri(path);
    d->searchPaths.insert(group, SearchPath(flags, *uriCopy));

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

ResourceNamespace::SearchPaths const& ResourceNamespace::searchPaths() const
{
    return d->searchPaths;
}

int ResourceNamespace::findAll(String name, ResourceList& found)
{
    int numFoundSoFar = found.count();

    NameHash::hash_type fromKey, toKey;
    if(!name.isEmpty())
    {
        fromKey = NameHash::hashName(name);
        toKey   = fromKey;
    }
    else
    {
        fromKey = 0;
        toKey   = NameHash::hash_range - 1;
    }

    for(NameHash::hash_type key = fromKey; key < toKey + 1; ++key)
    {
        NameHash::Bucket& bucket = d->nameHash.buckets[key];
        for(NameHash::Node* hashNode = bucket.first; hashNode; hashNode = hashNode->next)
        {
            ResourceRef& resource = hashNode->resource;
            PathTree::Node& node = resource.directoryNode();

            if(!name.isEmpty() && !node.name().beginsWith(name, Qt::CaseInsensitive)) continue;

            found.push_back(&node);
        }
    }

    return found.count() - numFoundSoFar;
}

#if _DEBUG
void ResourceNamespace::debugPrint() const
{
    LOG_AS("ResourceNamespace::debugPrint");
    LOG_DEBUG("[%p]:") << de::dintptr(this);

    uint resIdx = 0;
    for(NameHash::hash_type key = 0; key < NameHash::hash_range; ++key)
    {
        NameHash::Bucket& bucket = d->nameHash.buckets[key];
        for(NameHash::Node* node = bucket.first; node; node = node->next)
        {
            ResourceRef const& res = node->resource;

            LOG_DEBUG("  %u - %u:\"%s\" => %s")
                << resIdx << key << res.name()
                << NativePath(res.directoryNode().composePath()).pretty();

            ++resIdx;
        }
    }

    LOG_DEBUG("  %u %s in namespace.") << resIdx << (resIdx == 1? "resource" : "resources");
}
#endif

ResourceNamespace::SearchPath::SearchPath(int _flags, de::Uri& _uri)
    : flags_(_flags), uri_(&_uri)
{}

ResourceNamespace::SearchPath::SearchPath(SearchPath const& other)
    : flags_(other.flags_), uri_(new de::Uri(*other.uri_))
{}

ResourceNamespace::SearchPath::~SearchPath()
{
    delete uri_;
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

de::Uri const& ResourceNamespace::SearchPath::uri() const
{
    return *uri_;
}

void swap(ResourceNamespace::SearchPath& first, ResourceNamespace::SearchPath& second)
{
    std::swap(first.flags_,  second.flags_);
    std::swap(first.uri_,    second.uri_);
}

} // namespace de
