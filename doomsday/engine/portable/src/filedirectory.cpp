/**
 * @file filedirectory.cpp
 * FileDirectory implementation. @ingroup fs
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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
#include "de_filesys.h"

#include "resourcenamespace.h"
#include "timer.h"
#include <de/memory.h>

#include "filedirectory.h"

namespace de {

struct FileDirectory::Instance
{
    FileDirectory& self;

    /// Used with relative path directories.
    ddstring_t* basePath;

    /// Used with relative path directories.
    PathTree::Node* baseNode;

    Instance(FileDirectory& d, char const* _basePath)
        : self(d), basePath(0), baseNode(0)
    {
        if(_basePath && _basePath[0])
        {
            basePath = Str_Set(Str_NewStd(), _basePath);
            // Ensure path is correctly formed.
            F_AppendMissingSlash(basePath);
        }
    }

    ~Instance()
    {
        if(basePath) Str_Delete(basePath);
    }

    PathTree::Node* addPathNodes(ddstring_t const* rawPath)
    {
        if(!rawPath || Str_IsEmpty(rawPath)) return NULL;

        ddstring_t const* path;
        ddstring_t buf;
        if(basePath)
        {
            // Try to make it a relative path?
            if(F_IsAbsolute(rawPath))
            {
                F_RemoveBasePath2(Str_InitStd(&buf), rawPath, basePath);
                path = &buf;
            }
            else
            {
                path = rawPath;
            }

            // If this is equal to the base path, return that node.
            if(Str_IsEmpty(path))
            {
                // Time to construct the relative base node?
                // This node is purely symbolic, its only necessary for our internal use.
                if(!baseNode)
                {
                    baseNode = self.insert("./", '/');
                }

                if(path == &buf) Str_Free(&buf);
                return baseNode;
            }
        }
        else
        {
            // Do not add relative paths.
            path = rawPath;
            if(!F_IsAbsolute(path)) return NULL;
        }

        PathTree::Node* node = self.insert(Str_Text(path), '/');

        if(path == &buf) Str_Free(&buf);

        return node;
    }

    int addChildNodes(PathTree::Node& node, int flags,
        int (*callback) (PathTree::Node& node, void* parameters), void* parameters)
    {
        int result = 0; // Continue iteration.

        if(Branch == node.type())
        {
            // Compose the search pattern.
            ddstring_t searchPattern; Str_InitStd(&searchPattern);
            node.composePath(&searchPattern, NULL, '/');
            // We're interested in *everything*.
            Str_AppendChar(&searchPattern, '*');

            // Process this search.
            FS1::PathList found;
            if(App_FileSystem()->findAllPaths(Str_Text(&searchPattern), flags, found))
            {
                DENG2_FOR_EACH_CONST(FS1::PathList, i, found)
                {
                    QByteArray foundPathUtf8 = i->path.toUtf8();
                    ddstring_t foundPath; Str_InitStatic(&foundPath, foundPathUtf8.constData());
                    bool isDirectory = !!(i->attrib & A_SUBDIR);

                    result = addPathNodesAndMaybeDescendBranch(!(flags & SPF_NO_DESCEND), &foundPath, isDirectory,
                                                               flags, callback, parameters);
                    if(result) break;
                }
            }

            Str_Free(&searchPattern);
        }

        return result;
    }

    /**
     * @param filePath      Possibly-relative path to an element in the virtual file system.
     * @param flags         @ref searchPathFlags
     * @param callback      If not @c NULL the callback's logic dictates whether iteration continues.
     * @param parameters    Passed to the callback.
     * @param nodeType      Type of element, either a branch (directory) or a leaf (file).
     *
     * @return  Non-zero if the current iteration should stop else @c 0.
     */
    int addPathNodesAndMaybeDescendBranch(bool descendBranches,
        ddstring_t const* filePath, bool /*isDirectory*/,
        int flags, int (*callback) (PathTree::Node& node, void* parameters),
        void* parameters)
    {
        int result = 0; // Continue iteration.

        // Add this path to the directory.
        PathTree::Node* node = addPathNodes(filePath);
        if(node)
        {
            if(Branch == node->type())
            {
                // Descend into this subdirectory?
                if(descendBranches)
                {
                    // Already processed?
                    if(node->userValue())
                    {
                        // Does caller want to process it again?
                        if(callback)
                        {
                            DENG2_FOR_EACH_CONST(PathTree::Nodes, i, self.leafNodes())
                            {
                                if(node == (*i)->parent())
                                {
                                    result = callback(**i, parameters);
                                    if(result) break;
                                }
                            }

                            if(!result)
                            {
                                DENG2_FOR_EACH_CONST(PathTree::Nodes, i, self.branchNodes())
                                {
                                    if(node == (*i)->parent())
                                    {
                                        result = callback(**i, parameters);
                                        if(result) break;
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        result = addChildNodes(*node, flags, callback, parameters);

                        // This node is now considered processed.
                        node->setUserValue(true);
                    }
                }
            }
            // Node is a leaf.
            else if(callback)
            {
                result = callback(*node, parameters);

                // This node is now considered processed (if it wasn't already).
                node->setUserValue(true);
            }
        }

        return result;
    }
};

FileDirectory::FileDirectory(char const* basePath)
    : PathTree()
{
    d = new Instance(*this, basePath);
}

FileDirectory::~FileDirectory()
{
    delete d;
}

static void clearNodeProcessedFlags(PathTree::Nodes const& nodes)
{
    DENG2_FOR_EACH_CONST(PathTree::Nodes, i, nodes)
    {
        (*i)->setUserValue(false);
    }
}

void FileDirectory::clear()
{
    clearNodeProcessedFlags(leafNodes());
    clearNodeProcessedFlags(branchNodes());
    PathTree::clear();
    d->baseNode = NULL;
}

#if _DEBUG
static void printUriList(Uri const* const* pathList, size_t pathCount, int indent)
{
    if(!pathList) return;

    Uri const* const* pathsIt = pathList;
    for(size_t i = 0; i < pathCount && (*pathsIt); ++i, pathsIt++)
    {
        Uri_Print(*pathsIt, indent);
    }
}
#endif

void FileDirectory::addPath(int flags, Uri const* searchPath,
    int (*callback) (Node& node, void* parameters), void* parameters)
{
    if(!searchPath)
    {
        DEBUG_Message(("Warning: FileDirectory::AddPath: Attempt to add zero-length path, ignoring.\n"));
        return;
    }

#if _DEBUG
    VERBOSE2( Con_Message("Adding path to FileDirectory...\n");
              Uri_Print(searchPath, 2/*indent*/) )
#endif

    ddstring_t const* resolvedSearchPath = Uri_ResolvedConst(searchPath);
    if(!resolvedSearchPath) return;

    // Add new nodes on this path and/or re-process previously seen nodes.
    d->addPathNodesAndMaybeDescendBranch(true/*do descend*/, resolvedSearchPath, true/*is-directory*/,
                                         flags, callback, parameters);
}

void FileDirectory::addPaths(int flags,
    Uri const* const* searchPaths, uint searchPathsCount,
    int (*callback) (Node& node, void* parameters), void* parameters)
{
    if(!searchPaths || searchPathsCount == 0)
    {
        DEBUG_Message(("Warning: FileDirectory::AddPaths: Attempt to add zero-sized path list, ignoring.\n"));
        return;
    }

#if _DEBUG
    VERBOSE2( Con_Message("Adding paths to FileDirectory...\n");
              printUriList(searchPaths, searchPathsCount, 2/*indent*/) )
#endif

    for(uint i = 0; i < searchPathsCount; ++i)
    {
        ddstring_t const* searchPath = Uri_ResolvedConst(searchPaths[i]);
        if(!searchPath) continue;

        // Add new nodes on this path and/or re-process previously seen nodes.
        d->addPathNodesAndMaybeDescendBranch(true/*do descend*/, searchPath, true/*is-directory*/,
                                             flags, callback, parameters);
    }
}

} // namespace de
