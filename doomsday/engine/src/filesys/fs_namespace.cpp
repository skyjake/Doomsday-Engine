/**
 * @file filenamespace.cpp
 *
 * File namespace. @ingroup fs
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

#include <QDir>

#include "de_base.h"
#include "de_filesys.h"

#include <de/App>
#include <de/Log>
#include <de/NativePath>

#include "pathtree.h"

#include "filesys/searchpath.h"
#include "filesys/fs_main.h"

namespace de {

/**
 * Reference to a file in the virtual file system.
 */
class FileRef
{
public:
    FileRef(PathTree::Node& _directoryNode)
        : directoryNode_(&_directoryNode)
    {}

    PathTree::Node& directoryNode() const {
        return *directoryNode_;
    }

    FileRef& setDirectoryNode(PathTree::Node& newDirectoryNode) {
        directoryNode_ = &newDirectoryNode;
        return *this;
    }

#if _DEBUG
    String const& name() const {
        return name_;
    }

    FileRef& setName(String const& newName) {
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
        FileRef fileRef;

        Node(PathTree::Node& resourceNode)
            : next(0), fileRef(resourceNode)
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
        while(node && &node->fileRef.directoryNode() != &directoryNode)
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

struct FS1::Namespace::Instance
{
    FS1::Namespace& self;

    /// Symbolic name.
    String name;

    /// Flags which govern behavior.
    FS1::Namespace::Flags flags;

    /// Associated path directory.
    /// @todo It should not be necessary for a unique directory per namespace.
    PathTree directory;

    /// As the directory is relative, this special node serves as the root.
    PathTree::Node* rootNode;

    /// Name hash table.
    NameHash nameHash;

    /// Set to @c true when the name hash is obsolete/out-of-date and should be rebuilt.
    byte nameHashIsDirty;

    /// Sets of search paths to look for files to be included.
    /// Each set is in order of greatest-importance, right to left.
    FS1::Namespace::SearchPaths searchPaths;

    Instance(FS1::Namespace& d, String _name, FS1::Namespace::Flags _flags)
        : self(d), name(_name), flags(_flags), directory(), rootNode(0),
          nameHash(), nameHashIsDirty(true)
    {}

    /**
     * Add files to this namespace by resolving search path, searching the
     * file system and populating our internal directory with the results.
     * Duplicates are automatically pruned.
     *
     * @param searchPath  The path to resolve and search.
     */
    void addFromSearchPath(SearchPath const& searchPath)
    {
        try
        {
            // Add new nodes on this path and/or re-process previously seen nodes.
            addDirectoryPathNodesAndMaybeDescendBranch(true/*do descend*/, searchPath.resolved(),
                                                       true/*is-directory*/, searchPath.flags());
        }
        catch(de::Uri::ResolveError const& er)
        {
            LOG_DEBUG(er.asText());
        }
    }

    /**
     * Add files to this namespace by resolving each search path in @a group.
     *
     * @param group  Group of paths to search.
     */
    void addFromSearchPaths(FS1::PathGroup group)
    {
        for(FS1::Namespace::SearchPaths::const_iterator i = searchPaths.find(group);
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
            const String basePath = App_BasePath();
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
            addDirectoryPathNodesAndMaybeDescendBranch(!(flags & SearchPath::NoDescend),
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

FS1::Namespace::Namespace(String symbolicName, Flags flags)
{
    d = new Instance(*this, symbolicName, flags);
}

FS1::Namespace::~Namespace()
{
    delete d;
}

String const& FS1::Namespace::name() const
{
    return d->name;
}

void FS1::Namespace::clear()
{
    d->nameHash.clear();
    d->nameHashIsDirty = true;
    d->directory.clear();
    d->rootNode = 0;
}

void FS1::Namespace::rebuild()
{
    // Is a rebuild not necessary?
    if(!d->nameHashIsDirty) return;

    LOG_AS("FS1::Namespace::rebuild");
    LOG_DEBUG("Rebuilding '%s'...") << d->name;

    // uint startTime = Timer_RealMilliseconds();

    // (Re)populate the directory and add found files.
    clear();
    d->addFromSearchPaths(FS1::OverridePaths);
    d->addFromSearchPaths(FS1::ExtraPaths);
    d->addFromSearchPaths(FS1::DefaultPaths);
    d->addFromSearchPaths(FS1::FallbackPaths);

    d->nameHashIsDirty = false;

    // LOG_INFO("Done in %.2f seconds.") << (Timer_RealMilliseconds() - startTime) / 1000.0f;

/*#if _DEBUG
    PathTree::debugPrint(d->directory);
    debugPrint();
#endif*/
}

static inline String composeNamespaceName(String const& filePath)
{
    return filePath.fileNameWithoutExtension();
}

bool FS1::Namespace::add(PathTree::Node& resourceNode)
{
    // We are only interested in leafs (i.e., files and not folders).
    if(!resourceNode.isLeaf()) return false;

    String name = composeNamespaceName(resourceNode.name());
    NameHash::hash_type hashKey = NameHash::hashName(name);

    // Is this a new file?
    bool isNewNode = false;
    NameHash::Node* hashNode = d->nameHash.findDirectoryNode(hashKey, resourceNode);
    if(!hashNode)
    {
        isNewNode = true;

        // Create a new hash node for this.
        hashNode = new NameHash::Node(resourceNode);
#if _DEBUG
        // Take a copy of the name to aid in tracing bugs, etc...
        hashNode->fileRef.setName(name);
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
    FileRef* fileRef = &hashNode->fileRef;
    fileRef->setDirectoryNode(resourceNode);

    return isNewNode;
}

static String const& nameForPathGroup(FS1::PathGroup group)
{
    static String const names[] = {
        "Override",
        "Extra",
        "Default",
        "Fallback"
    };
    DENG_ASSERT(int(group) >= FS1::OverridePaths && int(group) <= FS1::FallbackPaths);
    return names[int(group)];
}

bool FS1::Namespace::addSearchPath(PathGroup group, SearchPath const& search)
{
    LOG_AS("FS1::Namespace::addSearchPath");

    // Ensure this is a well formed path.
    if(search.isEmpty() ||
       !search.path().compareWithoutCase("/") ||
       !search.path().endsWith("/"))
        return false;

    // The addition of a new search path means the namespace is now dirty.
    d->nameHashIsDirty = true;

    // Have we seen this path already (we don't want duplicates)?
    DENG2_FOR_EACH(SearchPaths, i, d->searchPaths)
    {
        // Compare using the unresolved textual representations.
        if(!i->asText().compareWithoutCase(search.asText()))
        {
            i->setFlags(search.flags());
            return true;
        }
    }

    // Prepend to the path list - newer paths have priority.
    d->searchPaths.insert(group, search);

    LOG_DEBUG("'%s' path \"%s\" added to namespace '%s'.")
        << nameForPathGroup(group) << search << name();

    return true;
}

void FS1::Namespace::clearSearchPaths(PathGroup group)
{
    d->searchPaths.remove(group);
}

void FS1::Namespace::clearSearchPaths()
{
    d->searchPaths.clear();
}

FS1::Namespace::SearchPaths const& FS1::Namespace::searchPaths() const
{
    return d->searchPaths;
}

int FS1::Namespace::findAll(String name, FileList& found)
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
            FileRef& fileRef = hashNode->fileRef;
            PathTree::Node& node = fileRef.directoryNode();

            if(!name.isEmpty() && !node.name().beginsWith(name, Qt::CaseInsensitive)) continue;

            found.push_back(&node);
        }
    }

    return found.count() - numFoundSoFar;
}

bool FS1::Namespace::applyPathMappings(String& path) const
{
    if(path.isEmpty()) return false;

    // Are virtual path mappings in effect for this namespace?
    if(!(d->flags & MappedInPackages)) return false;

    // Does this path qualify for mapping?
    if(path.length() <= name().length()) return false;
    if(path.at(name().length()) != '/')  return false;
    if(!path.beginsWith(name(), Qt::CaseInsensitive)) return false;

    // Yes.
    path = String("$(App.DataPath)/$(GamePlugin.Name)") / path;
    return true;
}

#if _DEBUG
void FS1::Namespace::debugPrint() const
{
    LOG_AS("FS1::Namespace::debugPrint");
    LOG_DEBUG("[%p]:") << de::dintptr(this);

    uint namespaceIdx = 0;
    for(NameHash::hash_type key = 0; key < NameHash::hash_range; ++key)
    {
        NameHash::Bucket& bucket = d->nameHash.buckets[key];
        for(NameHash::Node* node = bucket.first; node; node = node->next)
        {
            FileRef const& fileRef = node->fileRef;

            LOG_DEBUG("  %u - %u:\"%s\" => %s")
                << namespaceIdx << key << fileRef.name()
                << NativePath(fileRef.directoryNode().composePath()).pretty();

            ++namespaceIdx;
        }
    }

    LOG_DEBUG("  %u %s in namespace.") << namespaceIdx << (namespaceIdx == 1? "file" : "files");
}
#endif

} // namespace de
