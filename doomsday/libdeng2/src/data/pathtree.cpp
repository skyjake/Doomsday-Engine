/** @file pathtree.cpp Tree of Path/data pairs.
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

#include "de/Error"
#include "de/Log"
#include "de/StringPool"
#include "de/PathTree"

#include <QDebug>

namespace de {

Path::hash_type const PathTree::no_hash = Path::hash_range;

struct PathTree::Instance
{
    PathTree &self;

    /// Path name segment intern pool.
    StringPool segments;

    /// @see pathTreeFlags
    PathTree::Flags flags;

    /// Total number of unique paths in the directory.
    int size;

    /// Node that represents the one root branch of all nodes.
    PathTree::Node rootNode;

    /// Path node hashes.
    PathTree::Nodes leafHash;
    PathTree::Nodes branchHash;

    Instance(PathTree &d, int _flags)
        : self(d), flags(_flags), size(0),
          rootNode(PathTree::NodeArgs(d, PathTree::Branch, 0))
    {}

    ~Instance()
    {
        clear();
    }

    void clear()
    {
        clearPathHash(leafHash);
        clearPathHash(branchHash);
        size = 0;
    }

    PathTree::SegmentId internSegmentAndUpdateIdHashMap(String segment, Path::hash_type hashKey)
    {
        PathTree::SegmentId internId = segments.intern(segment);
        segments.setUserValue(internId, hashKey);
        return internId;
    }

    /**
     * @return Tree node that matches the name and type and which has the
     * specified parent node.
     */
    PathTree::Node *nodeForSegment(Path::Segment const &segment, PathTree::NodeType nodeType,
                                   PathTree::Node *parent)
    {
        // Have we already encountered this?
        PathTree::SegmentId segmentId = segments.isInterned(segment);
        if(segmentId)
        {
            // The name is known. Perhaps we have.
            PathTree::Nodes &hash = (nodeType == PathTree::Leaf? leafHash : branchHash);
            Path::hash_type hashKey = segments.userValue(segmentId);
            for(PathTree::Nodes::const_iterator i = hash.find(hashKey);
                i != hash.end() && i.key() == hashKey; ++i)
            {
                PathTree::Node *node = *i;
                if(parent    != &node->parent()) continue;
                if(segmentId != node->segmentId()) continue;

                if(nodeType == PathTree::Branch || !(flags & PathTree::MultiLeaf))
                    return node;
            }
        }

        /*
         * A new node is needed.
         */

        // Do we need a new identifier (and hash)?
        Path::hash_type hashKey;
        if(!segmentId)
        {
            hashKey   = segment.hash();
            segmentId = internSegmentAndUpdateIdHashMap(segment, hashKey);
        }
        else
        {
            hashKey = self.segmentHash(segmentId);
        }

        PathTree::Node *node = self.newNode(PathTree::NodeArgs(self, nodeType, segmentId, parent));

        // Insert the new node into the hash.
        if(nodeType == PathTree::Leaf)
        {
            leafHash.insert(hashKey, node);
        }
        else // Branch
        {
            branchHash.insert(hashKey, node);
        }

        return node;
    }

    /**
     * The path is split into as many nodes as necessary. Parent links are set.
     *
     * @return  The node that identifies the given path.
     */
    PathTree::Node *buildNodesForPath(Path const &path)
    {
        bool const hasLeaf = !path.toStringRef().endsWith("/");

        PathTree::Node *node = 0, *parent = &rootNode;
        for(int i = 0; i < path.segmentCount() - (hasLeaf? 1 : 0); ++i)
        {
            Path::Segment const &pn = path.segment(i);
            //qDebug() << "Add branch: " << pn.toString();
            node = nodeForSegment(pn, PathTree::Branch, parent);
            parent = node;
        }

        if(hasLeaf)
        {
            Path::Segment const &pn = path.lastSegment();
            //qDebug() << "Add leaf: " << pn.toString();
            node = nodeForSegment(pn, PathTree::Leaf, parent);
        }
        return node;
    }

    PathTree::Node *findInHash(PathTree::Nodes &hash, Path::hash_type hashKey,
                               Path const &searchPath,
                               PathTree::ComparisonFlags compFlags)
    {
        for(Nodes::iterator i = hash.find(hashKey);
            i != hash.end() && i.key() == hashKey; ++i)
        {
            PathTree::Node *node = *i;
            if(!node->comparePath(searchPath, compFlags))
            {
                // This is the leaf node we're looking for.
                if(compFlags.testFlag(RelinquishMatching))
                {
                    node->parent().removeChild(*node);
                    hash.erase(i);
                }
                return node;
            }
        }
        return 0;
    }

    PathTree::Node *find(Path const &searchPath, PathTree::ComparisonFlags compFlags)
    {
        if(searchPath.isEmpty()) return &rootNode;

        PathTree::Node *found = 0;
        if(size)
        {
            Path::hash_type hashKey = searchPath.lastSegment().hash();

            if(!compFlags.testFlag(NoLeaf))
            {
                if((found = findInHash(leafHash, hashKey, searchPath, compFlags)) != 0)
                    return found;
            }

            if(!compFlags.testFlag(NoBranch))
            {
                if((found = findInHash(branchHash, hashKey, searchPath, compFlags)) != 0)
                    return found;
            }
        }

        // The referenced node could not be found.
        return 0;
    }

    static void clearPathHash(PathTree::Nodes &ph)
    {
        LOG_AS("PathTree::clearPathHash");

        DENG2_FOR_EACH(PathTree::Nodes, i, ph)
        {
            PathTree::Node *node = *i;
            delete node;
        }
        ph.clear();
    }
};

PathTree::Node &PathTree::insert(Path const &path)
{
    PathTree::Node *node = d->buildNodesForPath(path);
    DENG2_ASSERT(node != 0);

    // There is now one more unique path in the tree.
    d->size++;

    return *node;
}

bool PathTree::remove(Path const &path, ComparisonFlags flags)
{
    PathTree::Node *node = d->find(path, flags | RelinquishMatching);
    if(node)
    {
        delete node;

        // One less unique path in the tree.
        d->size--;
        return true;
    }
    return false;
}

PathTree::PathTree(Flags flags)
{
    d = new Instance(*this, flags);
}

PathTree::~PathTree()
{
    delete d;
}

String const &PathTree::nodeTypeName(NodeType type)
{
    static String const nodeNames[] = {
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

PathTree::Flags PathTree::flags() const
{
    return d->flags;
}

void PathTree::clear()
{
    d->clear();
}

bool PathTree::has(Path const &path, ComparisonFlags flags) const
{    
    flags &= ~RelinquishMatching; // never relinquish
    return d->find(path, flags) != 0;
}

PathTree::Node const &PathTree::find(Path const &searchPath, ComparisonFlags flags) const
{
    Node const *found = d->find(searchPath, flags);
    if(!found)
    {
        /// @throw NotFoundError  The referenced node could not be found.
        throw NotFoundError("PathTree::find", "No paths found matching \"" + searchPath + "\"");
    }
    return *found;
}

PathTree::Node &PathTree::find(Path const &path, ComparisonFlags flags)
{
    Node const &node = const_cast<PathTree const *>(this)->find(path, flags);
    return const_cast<Node &>(node);
}

String const &PathTree::segmentName(SegmentId segmentId) const
{
    return d->segments.stringRef(segmentId);
}

Path::hash_type PathTree::segmentHash(SegmentId segmentId) const
{
    return d->segments.userValue(segmentId);
}

PathTree::Node const &PathTree::rootBranch() const
{
    return d->rootNode;
}

PathTree::Node *PathTree::newNode(NodeArgs const &args)
{
    return new Node(args);
}

PathTree::Nodes const &PathTree::nodes(NodeType type) const
{
    return (type == Leaf? d->leafHash : d->branchHash);
}

static void collectPathsInHash(PathTree::FoundPaths &found, PathTree::Nodes const &ph, QChar separator)
{
    if(ph.empty()) return;

    DENG2_FOR_EACH_CONST(PathTree::Nodes, i, ph)
    {
        PathTree::Node const &node = **i;
        found.push_back(node.path(separator));
    }
}

int PathTree::findAllPaths(FoundPaths &found, ComparisonFlags flags, QChar separator) const
{
    int numFoundSoFar = found.count();
    if(!(flags & NoBranch))
    {
        collectPathsInHash(found, branchNodes(), separator);
    }
    if(!(flags & NoLeaf))
    {
        collectPathsInHash(found, leafNodes(), separator);
    }
    return found.count() - numFoundSoFar;
}

static int iteratePathsInHash(PathTree const &pathTree, Path::hash_type hashKey,
                              PathTree::NodeType type, PathTree::ComparisonFlags flags,
                              PathTree::Node const *parent,
                              int (*callback) (PathTree::Node &, void *), void *parameters)
{
    int result = 0;

    if(hashKey != PathTree::no_hash && hashKey >= Path::hash_range)
    {
        throw Error("PathTree::iteratePathsInHash", String("Invalid hash %1, valid range is [0..%2).").arg(hashKey).arg(Path::hash_range-1));
    }

    PathTree::Nodes const *nodes = &pathTree.nodes(type);

    // Are we iterating nodes with a known hash?
    if(hashKey != PathTree::no_hash)
    {
        // Yes.
        PathTree::Nodes::const_iterator i = nodes->constFind(hashKey);
        for(; i != nodes->end() && i.key() == hashKey; ++i)
        {
            if(!(flags.testFlag(PathTree::MatchParent) && parent != &(*i)->parent()))
            {
                result = callback(**i, parameters);
                if(result) break;
            }
        }
    }
    else
    {
        // No known hash, but if the parent is known, we can narrow our search
        // to all the parent's children.
        if(!pathTree.flags().testFlag(PathTree::NoLocalBranchIndex) &&
           flags.testFlag(PathTree::MatchParent) && parent)
        {
            nodes = &parent->children();
        }

        // No known hash -- iterate all potential nodes.
        DENG2_FOR_EACH_CONST(PathTree::Nodes, i, *nodes)
        {
            if(flags.testFlag(PathTree::MatchParent) && (*i)->type() != type)
            {
                // Wrong kind of node -- branches could maintain separate indexes
                // for branches and leaves...
                continue;
            }

            if(!(flags.testFlag(PathTree::MatchParent) && parent != &(*i)->parent()))
            {
                result = callback(**i, parameters);
                if(result) break;
            }
        }
    }
    return result;
}

int PathTree::traverse(ComparisonFlags flags, PathTree::Node const *parent, Path::hash_type hashKey,
                       int (*callback) (PathTree::Node &, void *), void *parameters) const
{
    int result = 0;
    if(callback)
    {
        if(!(flags & NoLeaf))
            result = iteratePathsInHash(*this, hashKey, Leaf, flags, parent, callback, parameters);

        if(!result && !(flags & NoBranch))
            result = iteratePathsInHash(*this, hashKey, Branch, flags, parent, callback, parameters);
    }
    return result;
}

#ifdef DENG2_DEBUG
void PathTree::debugPrint(QChar separator) const
{
    LOG_AS("PathTree");
    LOG_INFO("[%p]:") << de::dintptr(this);
    FoundPaths found;
    if(findAllPaths(found, 0, separator))
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
static void printDistributionOverviewElement(int const *colWidths, char const *name,
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

        coverage  = 100 / (float)Path::hash_range * (Path::hash_range - numEmpty);
        collision = 100 / (float) total * numCollisions;
    }
    else
    {
        variance = coverage = collision = 0;
    }

    int const *col = colWidths;
    Con_Printf("%*s ",    *col++, name);
    Con_Printf("%*lu ",   *col++, (unsigned long)total);
    Con_Printf("%*lu",    *col++, Path::hash_range - (unsigned long)numEmpty);
    Con_Printf(":%-*lu ", *col++, (unsigned long)numEmpty);
    Con_Printf("%*lu ",   *col++, (unsigned long)maxCollisions);
    Con_Printf("%*lu ",   *col++, (unsigned long)numCollisions);
    Con_Printf("%*.2f ",  *col++, collision);
    Con_Printf("%*.2f ",  *col++, coverage);
    Con_Printf("%*.2f ",  *col++, variance);
    Con_Printf("%*lu\n",  *col++, (unsigned long)maxHeight);
}

static void printDistributionOverview(PathTree *pt,
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
    int *col = colWidths;
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
    int *span = &spans[0][0];
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
static void printDistributionHistogram(PathTree *pt, ushort size,
    size_t nodeCountTotal[PATHTREENODE_TYPE_COUNT])
{
#define NUMCOLS             4/*range+total+PATHTREENODE_TYPE_COUNT*/

    size_t totalForRange, total, nodeCount[PATHTREENODE_TYPE_COUNT];
    int hashIndexDigits, col, colWidths[2+/*range+total*/PATHTREENODE_TYPE_COUNT];
    PathTreeNode *node;
    int j;
    DENG2_ASSERT(pt);

    total = 0;
    for(int i = 0; i < PATHTREENODE_TYPE_COUNT; ++i)
    {
        total += nodeCountTotal[i];
    }
    if(0 == total) return;

    // Calculate minimum field widths:
    hashIndexDigits = M_NumDigits(Path::hash_range);
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

    { Path::hash_type from = 0, n = 0, range = (size != 0? Path::hash_range / size: 0);
    memset(nodeCount, 0, sizeof(nodeCount));

    for(Path::hash_type i = 0; i < Path::hash_range; ++i)
    {
        pathtree_pathhash_t **phAdr;
        phAdr = hashAddressForNodeType(pt, PathTree::Node::Branch);
        if(*phAdr)
        for(node = (**phAdr)[i].head; node; node = node->next)
            ++nodeCount[PathTree::Node::Branch];

        phAdr = hashAddressForNodeType(pt, PT_LEAF);
        if(*phAdr)
        for(node = (**phAdr)[i].head; node; node = node->next)
            ++nodeCount[PT_LEAF];

        if(size != 0 && (++n != range && i != Path::hash_range-1))
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

void PathTree::debugPrintHashDistribution() const
{
#if 0
    size_t nodeCountSum[PATHTREENODE_TYPE_COUNT],
           nodeCountTotal[PATHTREENODE_TYPE_COUNT], nodeBucketHeight = 0,
           nodeBucketCollisions[PATHTREENODE_TYPE_COUNT], nodeBucketCollisionsTotal = 0,
           nodeBucketCollisionsMax[PATHTREENODE_TYPE_COUNT], nodeBucketCollisionsMaxTotal = 0,
           nodeBucketEmpty[PATHTREENODE_TYPE_COUNT], nodeBucketEmptyTotal = 0,
           nodeCount[PATHTREENODE_TYPE_COUNT];
    size_t totalForRange;
    PathTreeNode *node;
    DENG2_ASSERT(pt);

    nodeCountTotal[PathTree::Node::Branch] = countNodesInPathHash(*hashAddressForNodeType(pt, PathTree::Node::Branch));
    nodeCountTotal[PT_LEAF]   = countNodesInPathHash(*hashAddressForNodeType(pt, PT_LEAF));

    memset(nodeCountSum, 0, sizeof(nodeCountSum));
    memset(nodeBucketCollisions, 0, sizeof(nodeBucketCollisions));
    memset(nodeBucketCollisionsMax, 0, sizeof(nodeBucketCollisionsMax));
    memset(nodeBucketEmpty, 0, sizeof(nodeBucketEmpty));

    for(Path::hash_type i = 0; i < Path::hash_range; ++i)
    {
        pathtree_pathhash_t **phAdr;
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
                PathTreeNode *other = node;

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
