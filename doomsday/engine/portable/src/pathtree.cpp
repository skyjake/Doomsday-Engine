/**
 * @file pathtree.cpp
 * @ingroup base
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

#if _DEBUG
#  include <cstdlib>
#endif

#include <de/Error>
#include <de/Log>
#include <de/stringpool.h>
#if 0
#  include "blockset.h"
#endif
#if _DEBUG
#  include "m_misc.h" // For M_NumDigits()
#endif

#include "pathtree.h"

namespace de {

#if 0
static volatile uint pathTreeInstanceCount;

/// A mutex is used to prevent Data races in the node allocator.
static mutex_t nodeAllocator_Mutex;

/// Threaded access to the following Data is protected by nodeAllocator_Mutex:
/// Nodes are block-allocated from this set.
static blockset_t* NodeBlockSet;
/// Linked list of used directory nodes for re-use. Linked with PathTreeNode::next
static PathTreeNode* UsedNodes;
#endif

struct PathTree::Instance
{
    PathTree& self;

    /// Path name fragment intern pool.
    StringPool fragments;

    /// @see pathTreeFlags
    int flags;

    /// Total number of unique paths in the directory.
    int size;

    /// Path node hashes.
    PathTree::Nodes leafHash;
    PathTree::Nodes branchHash;

    Instance(PathTree& d, int _flags)
        : self(d), fragments(), flags(_flags), size(0)
    {
#if 0
        // We'll block-allocate nodes and maintain a list of unused ones
        // to accelerate directory construction/population.
        if(!nodeAllocator_Mutex)
        {
            nodeAllocator_Mutex = Sys_CreateMutex("PathTreeNodeAllocator_MUTEX");

            Sys_Lock(nodeAllocator_Mutex);
            NodeBlockSet = BlockSet_New(sizeof(PathTreeNode), 128);
            UsedNodes = NULL;
            Sys_Unlock(nodeAllocator_Mutex);
        }

        pathTreeInstanceCount += 1;
#endif
    }

    ~Instance()
    {
        clear();
#if 0
        if(--pathTreeInstanceCount == 0)
        {
            Sys_Lock(nodeAllocator_Mutex);
            BlockSet_Delete(NodeBlockSet);
            NodeBlockSet = NULL;
            UsedNodes = NULL;
            Sys_DestroyMutex(nodeAllocator_Mutex);
            nodeAllocator_Mutex = 0;
        }
#endif
    }

    void clear()
    {
        clearPathHash(leafHash);
        clearPathHash(branchHash);
        size = 0;
    }

    PathTree::FragmentId internFragmentAndUpdateIdHashMap(ddstring_t const* fragment, ushort hash)
    {
        PathTree::FragmentId internId = fragments.intern(fragment);
        fragments.setUserValue(internId, hash);
        return internId;
    }

    /**
     * @return  [ a new | the ] directory node that matches the name and type and
     * which has the specified parent node.
     */
    PathTree::Node* direcNode(PathTree::Node* parent, PathTree::NodeType nodeType,
        ddstring_t const* fragment, char delimiter)
    {
        DENG2_ASSERT(fragment);

        // Have we already encountered this?
        PathTree::FragmentId fragmentId = fragments.isInterned(fragment);
        if(fragmentId)
        {
            // The name is known. Perhaps we have.
            PathTree::Nodes& hash = (nodeType == PathTree::Leaf? leafHash : branchHash);
            ushort hashKey = fragments.userValue(fragmentId);
            for(PathTree::Nodes::const_iterator i = hash.find(hashKey); i != hash.end() && i.key() == hashKey; ++i)
            {
                PathTree::Node* node = *i;
                if(parent     != node->parent()) continue;
                if(fragmentId != node->fragmentId()) continue;

                if(nodeType == PathTree::Branch || !(flags & PATHTREE_MULTI_LEAF))
                    return node;
            }
        }

        /*
         * A new node is needed.
         */

        // Do we need a new identifier (and hash)?
        ushort hash;
        if(!fragmentId)
        {
            hash = hashPathFragment(Str_Text(fragment), Str_Length(fragment), delimiter);
            fragmentId = internFragmentAndUpdateIdHashMap(fragment, hash);
        }
        else
        {
            hash = self.fragmentHash(fragmentId);
        }

        // Are we out of indices?
        if(!fragmentId) return NULL;

        PathTree::Node* node = newNode(self, nodeType, parent, fragmentId);

        // Insert the new node into the hash.
        if(nodeType == PathTree::Leaf)
        {
            leafHash.insert(hash, node);
        }
        else // Branch
        {
            branchHash.insert(hash, node);
        }

        return node;
    }

    /**
     * The path is split into as many nodes as necessary. Parent links are set.
     *
     * @return  The node that identifies the given path.
     */
    PathTree::Node* buildDirecNodes(char const* path, char delimiter)
    {
        DENG2_ASSERT(path);

        PathTree::Node* node = NULL, *parent = NULL;

        // Continue splitting as long as there are parts.
        AutoStr* part = AutoStr_NewStd();
        char const* p = path;
        while((p = Str_CopyDelim2(part, p, delimiter, CDF_OMIT_DELIMITER))) // Get the next part.
        {
            node = direcNode(parent, PathTree::Branch, part, delimiter);
            parent = node;
        }

        if(!Str_IsEmpty(part))
        {
            node = direcNode(parent, PathTree::Leaf, part, delimiter);
        }

        return node;
    }

    static PathTree::Node* newNode(PathTree& pt, PathTree::NodeType type,
        PathTree::Node* parent, PathTree::FragmentId fragmentId)
    {
        PathTree::Node* node;
#if 0
        // Acquire a new node, either from the used list or the block allocator.
        Sys_Lock(nodeAllocator_Mutex);
        if(UsedNodes)
        {
            node = UsedNodes;
            UsedNodes = node->next;

            // Reconfigure the node.
            node->next = NULL;
            node->directory_ = directory;
            node->type_ = type;
            node->parent_ = parent;
            node->pair.internId = internId;
            node->pair.data = userData;
        }
        else
#endif
        {
            //void* element = BlockSet_Allocate(NodeBlockSet);
            node = new /*(element)*/ PathTree::Node(pt, type, fragmentId, parent);
        }
#if 0
        Sys_Unlock(nodeAllocator_Mutex);
#endif

        return node;
    }

    static void deleteNode(PathTree::Node& node)
    {
        delete &node;
#if 0
        // Add this node to the list of used nodes for re-use.
        Sys_Lock(nodeAllocator_Mutex);
        next = UsedNodes;
        UsedNodes = this;
        Sys_Unlock(nodeAllocator_Mutex);
#endif
    }

    static void clearPathHash(PathTree::Nodes& ph)
    {
        LOG_AS("PathTree::clearPathHash");

        DENG2_FOR_EACH(PathTree::Nodes, i, ph)
        {
            PathTree::Node& node = **i;
#if _DEBUG
            if(node.userPointer())
            {
                LOG_ERROR("Node %p has non-NULL user data.") << de::dintptr(&node);
            }
#endif
            deleteNode(node);
        }
        ph.clear();
    }
};

ushort PathTree::hashPathFragment(const char* fragment, size_t len, char delimiter)
{
    ushort key = 0;

    DENG2_ASSERT(fragment);

    // Skip over any trailing delimiters.
    const char* c = fragment + len - 1;
    while(c >= fragment && *c && *c == delimiter) c--;

    // Compose the hash.
    int op = 0;
    for(; c >= fragment && *c && *c != delimiter; c--)
    {
        switch(op)
        {
        case 0: key ^= tolower(*c); ++op;   break;
        case 1: key *= tolower(*c); ++op;   break;
        case 2: key -= tolower(*c);   op=0; break;
        }
    }
    return key % PATHTREE_PATHHASH_SIZE;
}

PathTree::Node* PathTree::insert(const char* path, char delimiter)
{
    PathTree::Node* node = d->buildDirecNodes(path, delimiter);
    if(node)
    {
        // There is now one more unique path in the directory.
        d->size += 1;
    }
    return node;
}

PathTree::PathTree(int flags)
{
    d = new Instance(*this, flags);
}

PathTree::~PathTree()
{
    delete d;
}

ddstring_t const* PathTree::nodeTypeName(NodeType type)
{
    static Str const nodeNames[] = {
        "branch",
        "leaf"
    };
    return nodeNames[type == Branch? 0 : 1];
}

int PathTree::size() const
{
    return d->size;
}

bool PathTree::empty() const
{
    return size() == 0;
}

void PathTree::clear()
{
    d->clear();
}

PathTree::Node& PathTree::find(int flags, char const* searchPath, char delimiter)
{
    Node* foundNode = NULL;
    if(searchPath && searchPath[0] && d->size)
    {
        PathMap mappedSearchPath = PathMap(hashPathFragment, searchPath, delimiter);

        ushort hash = PathMap_Fragment(&mappedSearchPath, 0)->hash;
        if(!(flags & PCF_NO_LEAF))
        {
            Nodes& nodes = d->leafHash;
            Nodes::iterator i = nodes.find(hash);
            for(; i != nodes.end() && i.key() == hash; ++i)
            {
                if((*i)->comparePath(mappedSearchPath, flags))
                {
                    // This is the node we're looking for - stop iteration.
                    foundNode = *i;
                    break;
                }
            }
        }

        if(!foundNode)
        if(!(flags & PCF_NO_BRANCH))
        {
            Nodes& nodes = d->branchHash;
            Nodes::iterator i = nodes.find(hash);
            for(; i != nodes.end() && i.key() == hash; ++i)
            {
                if((*i)->comparePath(mappedSearchPath, flags))
                {
                    // This is the node we're looking for - stop iteration.
                    foundNode = *i;
                    break;
                }
            }
        }
    }

    if(!foundNode) throw NotFoundError("PathTree::find", String("No paths found matching \"") + searchPath + "\"");
    return *foundNode;
}

ddstring_t const* PathTree::fragmentName(FragmentId fragmentId) const
{
    return d->fragments.string(fragmentId);
}

ushort PathTree::fragmentHash(FragmentId fragmentId) const
{
    return d->fragments.userValue(fragmentId);
}

PathTree::Nodes const& PathTree::nodes(NodeType type) const
{
    return (type == Leaf? d->leafHash : d->branchHash);
}

static void collectPathsInHash(PathTree::FoundPaths& found, PathTree::Nodes const& ph, char delimiter)
{
    if(ph.empty()) return;
    DENG2_FOR_EACH_CONST(PathTree::Nodes, i, ph)
    {
        PathTree::Node& node = **i;
        found.push_back(node.composePath(delimiter));
    }
}

int PathTree::findAllPaths(FoundPaths& found, int flags, char delimiter)
{
    int numFoundSoFar = found.count();
    if(!(flags & PCF_NO_BRANCH))
    {
        collectPathsInHash(found, branchNodes(), delimiter);
    }
    if(!(flags & PCF_NO_LEAF))
    {
        collectPathsInHash(found, leafNodes(), delimiter);
    }
    return found.count() - numFoundSoFar;
}

static int iteratePathsInHash(PathTree& pathTree, ushort hash, PathTree::NodeType type, int flags,
    PathTree::Node* parent, int (*callback) (PathTree::Node&, void*), void* parameters)
{
    int result = 0;

    if(hash != PATHTREE_NOHASH && hash >= PATHTREE_PATHHASH_SIZE)
    {
        throw Error("PathTree::iteratePathsInHash", String("Invalid hash %1 (valid range is [0..%2]).").arg(hash).arg(PATHTREE_PATHHASH_SIZE-1));
    }

    PathTree::Nodes const& nodes = pathTree.nodes(type);

    // Are we iterating nodes with a known hash?
    if(hash != PATHTREE_NOHASH)
    {
        // Yes.
        PathTree::Nodes::const_iterator i = nodes.constFind(hash);
        for(; i != nodes.end() && i.key() == hash; ++i)
        {
            if(!((flags & PCF_MATCH_PARENT) && parent != (*i)->parent()))
            {
                result = callback(**i, parameters);
                if(result) break;
            }
        }
    }
    else
    {
        // No - iterate all nodes.
        DENG2_FOR_EACH_CONST(PathTree::Nodes, i, nodes)
        {
            if(!((flags & PCF_MATCH_PARENT) && parent != (*i)->parent()))
            {
                result = callback(**i, parameters);
                if(result) break;
            }
        }
    }
    return result;
}

int PathTree::iterate(int flags, PathTree::Node* parent, ushort hash,
    int (*callback) (PathTree::Node&, void*), void* parameters)
{
    int result = 0;
    if(callback)
    {
        if(!(flags & PCF_NO_LEAF))
            result = iteratePathsInHash(*this, hash, Leaf, flags, parent, callback, parameters);

        if(!result && !(flags & PCF_NO_BRANCH))
            result = iteratePathsInHash(*this, hash, Branch, flags, parent, callback, parameters);
    }
    return result;
}

#if _DEBUG
void PathTree::debugPrint(PathTree& pt, char delimiter)
{
    LOG_AS("PathTree");
    LOG_INFO("[%p]:") << de::dintptr(&pt);
    FoundPaths found;
    if(pt.findAllPaths(found, 0, delimiter))
    {
        qSort(found.begin(), found.end());

        DENG2_FOR_EACH_CONST(FoundPaths, i, found)
        {
            LOG_INFO("  %s") << *i;
        }
    }
    LOG_INFO("  %i unique %s in the tree.") << found.count() << (found.count() == 1? "path" : "paths");
}

#if 0
static void printDistributionOverviewElement(const int* colWidths, const char* name,
    size_t numEmpty, size_t maxHeight, size_t numCollisions, size_t maxCollisions,
    size_t sum, size_t total)
{
    DENG2_ASSERT(colWidths);

    float coverage, collision, variance;
    if(0 != total)
    {
        size_t sumSqr = sum*sum;
        float mean = (signed)sum / total;
        variance = ((signed)sumSqr - (signed)sum * mean) / (((signed)total)-1);

        coverage  = 100 / (float)PATHTREE_PATHHASH_SIZE * (PATHTREE_PATHHASH_SIZE - numEmpty);
        collision = 100 / (float) total * numCollisions;
    }
    else
    {
        variance = coverage = collision = 0;
    }

    const int* col = colWidths;
    Con_Printf("%*s ",    *col++, name);
    Con_Printf("%*lu ",   *col++, (unsigned long)total);
    Con_Printf("%*lu",    *col++, PATHTREE_PATHHASH_SIZE - (unsigned long)numEmpty);
    Con_Printf(":%-*lu ", *col++, (unsigned long)numEmpty);
    Con_Printf("%*lu ",   *col++, (unsigned long)maxCollisions);
    Con_Printf("%*lu ",   *col++, (unsigned long)numCollisions);
    Con_Printf("%*.2f ",  *col++, collision);
    Con_Printf("%*.2f ",  *col++, coverage);
    Con_Printf("%*.2f ",  *col++, variance);
    Con_Printf("%*lu\n",  *col++, (unsigned long)maxHeight);
}

static void printDistributionOverview(PathTree* pt,
    size_t nodeCountSum[PATHTREENODE_TYPE_COUNT],
    size_t nodeCountTotal[PATHTREENODE_TYPE_COUNT],
    size_t nodeBucketCollisions[PATHTREENODE_TYPE_COUNT], size_t nodeBucketCollisionsTotal,
    size_t nodeBucketCollisionsMax[PATHTREENODE_TYPE_COUNT], size_t /*nodeBucketCollisionsMaxTotal*/,
    size_t nodeBucketEmpty[PATHTREENODE_TYPE_COUNT], size_t nodeBucketEmptyTotal, size_t nodeBucketHeight,
    size_t /*nodeCount*/[PATHTREENODE_TYPE_COUNT])
{
#define NUMCOLS             10/*type+count+used:+empty+collideMax+collideCount+collidePercent+coverage+variance+maxheight*/

    DENG2_ASSERT(pt);

    size_t collisionsMax = 0, countSum = 0, countTotal = 0;
    for(int i = 0; i < PATHTREENODE_TYPE_COUNT; ++i)
    {
        if(nodeBucketCollisionsMax[i] > collisionsMax)
            collisionsMax = nodeBucketCollisionsMax[i];
        countSum += nodeCountSum[i];
        countTotal += nodeCountTotal[i];
    }

    int nodeCountDigits = M_NumDigits((int)countTotal);

    // Calculate minimum field widths:
    int colWidths[NUMCOLS];
    int* col = colWidths;
    *col = 0;
    for(int i = 0; i < PATHTREENODE_TYPE_COUNT; ++i)
    {
        PathTree::NodeType type = PathTree::NodeType(i);
        if(Str_Length(PathTreeNode::typeName(type)) > *col)
            *col = Str_Length(PathTreeNode::typeName(type));
    }
    col++;
    *col++ = MAX_OF(nodeCountDigits, 1); /*#*/
    *col++ = MAX_OF(nodeCountDigits, 4); /*used*/
    *col++ = MAX_OF(nodeCountDigits, 5); /*empty*/
    *col++ = MAX_OF(nodeCountDigits, 3); /*max*/
    *col++ = MAX_OF(nodeCountDigits, 4); /*num*/
    *col++ = MAX_OF(3+1+2, 8);           /*percent*/
    *col++ = MAX_OF(3+1+2, 9);           /*coverage*/
    *col++ = MAX_OF(nodeCountDigits, 8); /*variance*/
    *col   = MAX_OF(nodeCountDigits, 9); /*maxheight*/

    // Calculate span widths:
    int spans[4][2];
    spans[0][0] = colWidths[0] + 1/* */ + colWidths[1];
    spans[1][0] = colWidths[2] + 1/*:*/ + colWidths[3];
    spans[2][0] = colWidths[4] + 1/* */ + colWidths[5] + 1/* */ + colWidths[6];
    spans[3][0] = colWidths[7] + 1/* */ + colWidths[8] + 1/* */ + colWidths[9];
    for(int i = 0; i < 4; ++i)
    {
        int remainder = spans[i][0] % 2;
        spans[i][1] = remainder + (spans[i][0] /= 2);
    }

    Con_FPrintf(CPF_YELLOW, "Directory Distribution (p:%p):\n", pt);

    // Level1 headings:
    int* span = &spans[0][0];
    Con_Printf("%*s", *span++ +  5/2, "nodes");         Con_Printf("%-*s|", *span++ -  5/2, "");
    Con_Printf("%*s", *span++ +  4/2, "hash");          Con_Printf("%-*s|", *span++ -  4/2, "");
    Con_Printf("%*s", *span++ + 10/2, "collisions");    Con_Printf("%-*s|", *span++ - 10/2, "");
    Con_Printf("%*s", *span++ +  5/2, "other");         Con_Printf("%-*s\n",*span++ -  5/2, "");

    // Level2 headings:
    col = colWidths;
    Con_FPrintf(CPF_LIGHT, "%*s ",   *col++, "type");
    Con_FPrintf(CPF_LIGHT, "%-*s|",  *col++, "#");
    Con_FPrintf(CPF_LIGHT, "%*s:",   *col++, "used");
    Con_FPrintf(CPF_LIGHT, "%-*s|",  *col++, "empty");
    Con_FPrintf(CPF_LIGHT, "%*s ",   *col++, "max");
    Con_FPrintf(CPF_LIGHT, "%*s ",   *col++, "num#");
    Con_FPrintf(CPF_LIGHT, "%-*s|",  *col++, "percent%");
    Con_FPrintf(CPF_LIGHT, "%*s ",   *col++, "coverage%");
    Con_FPrintf(CPF_LIGHT, "%*s ",   *col++, "variance");
    Con_FPrintf(CPF_LIGHT, "%-*s\n", *col++, "maxheight");

    if(countTotal != 0)
    {
        for(int i = 0; i < PATHTREENODE_TYPE_COUNT; ++i)
        {
            PathTree::NodeType type = PathTree::NodeType(i);
            printDistributionOverviewElement(colWidths, Str_Text(PathTreeNode::typeName(type)),
                nodeBucketEmpty[i], (i == PT_LEAF? nodeBucketHeight : 0),
                nodeBucketCollisions[i], nodeBucketCollisionsMax[i],
                nodeCountSum[i], nodeCountTotal[i]);
        }
        Con_PrintRuler();
    }

    printDistributionOverviewElement(colWidths, "total",
        nodeBucketEmptyTotal, nodeBucketHeight,
        nodeBucketCollisionsTotal, collisionsMax,
        countSum / PATHTREENODE_TYPE_COUNT, countTotal);

#undef NUMCOLS
}
#endif

#if 0
static void printDistributionHistogram(PathTree* pt, ushort size,
    size_t nodeCountTotal[PATHTREENODE_TYPE_COUNT])
{
#define NUMCOLS             4/*range+total+PATHTREENODE_TYPE_COUNT*/

    size_t totalForRange, total, nodeCount[PATHTREENODE_TYPE_COUNT];
    int hashIndexDigits, col, colWidths[2+/*range+total*/PATHTREENODE_TYPE_COUNT];
    PathTreeNode* node;
    int j;
    DENG2_ASSERT(pt);

    total = 0;
    for(int i = 0; i < PATHTREENODE_TYPE_COUNT; ++i)
    {
        total += nodeCountTotal[i];
    }
    if(0 == total) return;

    // Calculate minimum field widths:
    hashIndexDigits = M_NumDigits(PATHTREE_PATHHASH_SIZE);
    col = 0;
    if(size != 0)
        colWidths[col] = 2/*braces*/+hashIndexDigits*2+3/*elipses*/;
    else
        colWidths[col] = 2/*braces*/+hashIndexDigits;
    colWidths[col] = MAX_OF(colWidths[col], 5/*range*/);
    ++col;

    { size_t max = 0;
    for(int i = 0; i < PATHTREENODE_TYPE_COUNT; ++i)
    {
        if(nodeCountTotal[i] > max)
            max = nodeCountTotal[i];
    }
    colWidths[col++] = MAX_OF(M_NumDigits((int)max), 5/*total*/);
    }

    for(int i = 0; i < PATHTREENODE_TYPE_COUNT; ++i, ++col)
    {
        PathTree::NodeType type = PathTree::NodeType(i);
        colWidths[col] = Str_Length(PathTreeNode::typeName(type));
    }

    // Apply formatting:
    for(int i = 1; i < NUMCOLS; ++i) { colWidths[i] += 1; }

    Con_FPrintf(CPF_YELLOW, "Histogram (p:%p):\n", pt);
    // Print heading:
    col = 0;
    Con_Printf("%*s", colWidths[col++], "range");
    Con_Printf("%*s", colWidths[col++], "total");
    for(int i = 0; i < PATHTREENODE_TYPE_COUNT; ++i)
    {
        PathTree::NodeType type = PathTree::NodeType(i);
        Con_Printf("%*s", colWidths[col++], Str_Text(PathTreeNode::typeName(type)));
    }
    Con_Printf("\n");
    Con_PrintRuler();

    { ushort from = 0, n = 0, range = (size != 0? PATHTREE_PATHHASH_SIZE / size: 0);
    memset(nodeCount, 0, sizeof(nodeCount));

    for(ushort i = 0; i < PATHTREE_PATHHASH_SIZE; ++i)
    {
        pathtree_pathhash_t** phAdr;
        phAdr = hashAddressForNodeType(pt, PathTree::Node::Branch);
        if(*phAdr)
        for(node = (**phAdr)[i].head; node; node = node->next)
            ++nodeCount[PathTree::Node::Branch];

        phAdr = hashAddressForNodeType(pt, PT_LEAF);
        if(*phAdr)
        for(node = (**phAdr)[i].head; node; node = node->next)
            ++nodeCount[PT_LEAF];

        if(size != 0 && (++n != range && i != PATHTREE_PATHHASH_SIZE-1))
            continue;

        totalForRange = 0;
        for(j = 0; j < PATHTREENODE_TYPE_COUNT; ++j)
            totalForRange += nodeCount[j];

        col = 0;
        if(size != 0)
        {
            Str range; Str_Init(&range);
            Str_Appendf(&range, "%*u...%*u", hashIndexDigits, from, hashIndexDigits, from+n-1);
            Con_Printf("[%*s]", colWidths[col++]-2/*braces*/, Str_Text(&range));
            Str_Free(&range);
        }
        else
        {
            Con_Printf("[%*u]", colWidths[col++]-2/*braces*/, i);
        }

        Con_Printf("%*lu", colWidths[col++], (unsigned long) totalForRange);
        if(0 != totalForRange)
        {
            for(j = 0; j < PATHTREENODE_TYPE_COUNT; ++j, ++col)
            {
                if(0 != nodeCount[j])
                {
                    Con_Printf("%*lu", colWidths[col], (unsigned long) nodeCount[j]);
                }
                else if(j < PATHTREENODE_TYPE_COUNT-1 || 0 == size)
                {
                    Con_Printf("%*s", colWidths[col], "");
                }
            }
        }

        // Are we printing a "graphical" representation?
        if(0 != totalForRange)
        {
            size_t max = MAX_OF(1, ROUND(total/(float)size/10));
            size_t scale = totalForRange / (float)max;

            scale = MAX_OF(scale, 1);
            Con_Printf(" ");
            for(n = 0; n < scale; ++n)
                Con_Printf("*");
        }

        Con_Printf("\n");
        from = i+1;
        n = 0;
        memset(nodeCount, 0, sizeof(nodeCount));
    }}
    Con_PrintRuler();

    // Sums:
    col = 0;
    Con_Printf("%*s",  colWidths[col++], "Sum");
    Con_Printf("%*lu", colWidths[col++], (unsigned long) total);
    if(0 != total)
    {
        int i;
        for(i = 0; i < PATHTREENODE_TYPE_COUNT; ++i, ++col)
        {
            if(0 != nodeCountTotal[i])
            {
                Con_Printf("%*lu", colWidths[col], (unsigned long) nodeCountTotal[i]);
            }
            else if(i < PATHTREENODE_TYPE_COUNT-1)
            {
                Con_Printf("%*s", colWidths[col], "");
            }
        }
    }
    Con_Printf("\n");

#undef NUMCOLS
}
#endif

void PathTree::debugPrintHashDistribution(PathTree& /*pt*/)
{
#if 0
    size_t nodeCountSum[PATHTREENODE_TYPE_COUNT],
           nodeCountTotal[PATHTREENODE_TYPE_COUNT], nodeBucketHeight = 0,
           nodeBucketCollisions[PATHTREENODE_TYPE_COUNT], nodeBucketCollisionsTotal = 0,
           nodeBucketCollisionsMax[PATHTREENODE_TYPE_COUNT], nodeBucketCollisionsMaxTotal = 0,
           nodeBucketEmpty[PATHTREENODE_TYPE_COUNT], nodeBucketEmptyTotal = 0,
           nodeCount[PATHTREENODE_TYPE_COUNT];
    size_t totalForRange;
    PathTreeNode* node;
    DENG2_ASSERT(pt);

    nodeCountTotal[PathTree::Node::Branch] = countNodesInPathHash(*hashAddressForNodeType(pt, PathTree::Node::Branch));
    nodeCountTotal[PT_LEAF]   = countNodesInPathHash(*hashAddressForNodeType(pt, PT_LEAF));

    memset(nodeCountSum, 0, sizeof(nodeCountSum));
    memset(nodeBucketCollisions, 0, sizeof(nodeBucketCollisions));
    memset(nodeBucketCollisionsMax, 0, sizeof(nodeBucketCollisionsMax));
    memset(nodeBucketEmpty, 0, sizeof(nodeBucketEmpty));

    for(ushort i = 0; i < PATHTREE_PATHHASH_SIZE; ++i)
    {
        pathtree_pathhash_t** phAdr;
        phAdr = hashAddressForNodeType(pt, PathTree::Node::Branch);
        nodeCount[PathTree::Node::Branch] = 0;
        if(*phAdr)
        for(node = (**phAdr)[i].head; node; node = node->next)
            ++nodeCount[PathTree::Node::Branch];

        phAdr = hashAddressForNodeType(pt, PT_LEAF);
        nodeCount[PT_LEAF] = 0;
        if(*phAdr)
        {
            size_t chainHeight = 0;
            for(node = (**phAdr)[i].head; node; node = node->next)
            {
                size_t height = 0;
                PathTreeNode* other = node;

                ++nodeCount[PT_LEAF];

                while((other = other->parent())) { ++height; }

                if(height > chainHeight)
                    chainHeight = height;
            }

            if(chainHeight > nodeBucketHeight)
                nodeBucketHeight = chainHeight;
        }

        totalForRange = nodeCount[PathTree::Node::Branch] + nodeCount[PT_LEAF];

        nodeCountSum[PT_BRANCH] += nodeCount[PathTree::Node::Branch];
        nodeCountSum[PT_LEAF]   += nodeCount[PT_LEAF];

        for(int j = 0; j < PATHTREENODE_TYPE_COUNT; ++j)
        {
            if(nodeCount[j] != 0)
            {
                if(nodeCount[j] > 1)
                    nodeBucketCollisions[j] += nodeCount[j]-1;
            }
            else
            {
                ++(nodeBucketEmpty[j]);
            }
            if(nodeCount[j] > nodeBucketCollisionsMax[j])
                nodeBucketCollisionsMax[j] = nodeCount[j];
        }

        size_t max = 0;
        for(int j = 0; j < PATHTREENODE_TYPE_COUNT; ++j)
        {
            max += nodeCount[j];
        }
        if(max > nodeBucketCollisionsMaxTotal)
            nodeBucketCollisionsMaxTotal = max;

        if(totalForRange != 0)
        {
            if(totalForRange > 1)
                nodeBucketCollisionsTotal += totalForRange-1;
        }
        else
        {
            ++nodeBucketEmptyTotal;
        }
        if(totalForRange > nodeBucketCollisionsMaxTotal)
            nodeBucketCollisionsMaxTotal = totalForRange;
    }

    printDistributionOverview(pt, nodeCountSum, nodeCountTotal,
        nodeBucketCollisions,    nodeBucketCollisionsTotal,
        nodeBucketCollisionsMax, nodeBucketCollisionsMaxTotal,
        nodeBucketEmpty, nodeBucketEmptyTotal,
        nodeBucketHeight, nodeCount);
    Con_Printf("\n");
    printDistributionHistogram(pt, 16, nodeCountTotal);
#endif
}
#endif

} // namespace de
