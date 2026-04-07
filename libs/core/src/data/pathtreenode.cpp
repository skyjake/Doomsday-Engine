/** @file pathtreenode.cpp PathTree::Node implementation.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/pathtree.h"
#include "de/log.h"

namespace de {

DE_PIMPL_NOREF(PathTree::Node)
{
    /// PathTree which owns this node.
    PathTree &tree;

    /// Parent node in the user's hierarchy.
    Node *parent;

    /// @c NULL for leaves, index of children for branches.
    Node::Children *children;

    const LowercaseHashString segment;

    /// Unique identifier for the path fragment this node represents,
    /// in the owning PathTree.
//    SegmentId segmentId;

//    const String *segmentText = nullptr; // owned by the PathTree

    Impl(PathTree & _tree, bool isLeaf, const LowercaseHashString &segment, Node *_parent)
        : tree(_tree)
        , parent(_parent)
        , children(0)
        , segment(segment)
    {
        if (!isLeaf) children = new Node::Children;
    }

    ~Impl()
    {
        delete children;
    }

//    void cacheSegmentText()
//    {
//        segmentText = &tree.segmentName(segmentId);
//    }
};

PathTree::Node::Node(const NodeArgs &args) : d(nullptr)
{
    d.reset(new Impl(args.tree, args.type == Leaf, args.segment, args.parent));

    // Let the parent know of the new child node.
    if (d->parent) d->parent->addChild(*this);
}

PathTree::Node::~Node()
{}

bool PathTree::Node::isLeaf() const
{
    return d->children == 0;
}

PathTree &PathTree::Node::tree() const
{
    return d->tree;
}

PathTree::Node &PathTree::Node::parent() const
{
    return *d->parent;
}

const PathTree::Node::Children &PathTree::Node::children() const
{
    DE_ASSERT(d->children != 0);
    return *d->children;
}

const PathTree::Nodes &PathTree::Node::childNodes(NodeType type) const
{
    DE_ASSERT(d->children != 0);
    return (type == Leaf? d->children->leaves : d->children->branches);
}

PathTree::Nodes &PathTree::Node::childNodes(NodeType type)
{
    DE_ASSERT(d->children != 0);
    return (type == Leaf? d->children->leaves : d->children->branches);
}

bool PathTree::Node::isAtRootLevel() const
{
    return d->parent == &d->tree.rootBranch();
}

//PathTree::SegmentId PathTree::Node::segmentId() const
//{
//    return d->segmentId;
//}

void PathTree::Node::addChild(Node &node)
{
    DE_ASSERT(d->children != 0);

    childNodes(node.type()).insert(std::make_pair(node.key().hash, &node));
}

void PathTree::Node::removeChild(Node &node)
{
    DE_ASSERT(d->children != 0);

    auto &hash = childNodes(node.type());
    auto found = hash.equal_range(node.key().hash);
    for (auto i = found.first; i != found.second; ++i)
    {
        if (i->second == &node)
        {
            hash.erase(i);
            break;
        }
    }
}

const String &PathTree::Node::name() const
{
//    if (!d->segmentText)
//    {
//        // Cache the string, because PathTree::segmentName() locks the tree and that has
//        // performance implications. The segment text string will not change while the
//        // node exists.
//        d->cacheSegmentText();
//    }
    return key().str;
}

const LowercaseHashString &PathTree::Node::key() const
{
    return d->segment;
}

/*Path::hash_type PathTree::Node::hash() const
{
    return tree().segmentHash(d->segmentId);
}*/

/// @todo This logic should be encapsulated in de::Path or de::Path::Segment.
static int matchName(const CString &string, const CString &pattern)
{
    mb_iterator in     = string.begin();
    const char *inEnd  = string.end();
    mb_iterator pat    = pattern.begin();
    mb_iterator asterisk;

    while (in < inEnd)
    {
        if (*pat == '*')
        {
            asterisk = pat;
            pat++;
            continue;
        }

        if ((*pat).lower() != (*in).lower())
        {
            // A mismatch. Hmm. Go back to a previous '*'.
            //while (pat >= pattern && *pat != QChar('*')) { --pat; }
            if (!asterisk) return false;

            // The asterisk lets us continue.
            pat = asterisk;

            // No match?
            //if (pat < pattern) return false;

        }

        // This character of the pattern is OK.
        pat++;
        in++;
    }

    // Skip remaining asterisks.
    while (*pat == '*') { pat++; }

    // Match is good if the end of the pattern was reached.
    return pat == pattern.end();
}

int PathTree::Node::comparePath(const Path &searchPattern, ComparisonFlags flags) const
{
    if (((flags & NoLeaf)   && isLeaf()) ||
        ((flags & NoBranch) && isBranch()))
    {
        return 1;
    }

    const Path::Segment *snode = &searchPattern.lastSegment();

    // In reverse order, compare each path node in the search term.
    int pathNodeCount = searchPattern.segmentCount();

    const auto *node = this;
    for (int i = 0; i < pathNodeCount; ++i)
    {
        if (!snode->hasWildCard())
        {
            // If the hashes don't match it can't possibly be this.
//            if (snode->hash() != node->hash())
//            {
//                return 1;
//            }
//            if (node->name().compare(*snode, CaseInsensitive))
//            {
//                return 1;
//            }
            if (node->key() != snode->key())
            {
                return 1;
            }
        }
        else
        {
            // Compare the names using wildcard pattern matching.
            // Note: This has relatively slow performance.
            if (!matchName(node->name(), *snode))
            {
                return 1;
            }
        }

        // Have we arrived at the search target?
        if (i == pathNodeCount - 1)
        {
            return !(!(flags & MatchFull) || node->isAtRootLevel());
        }

        // Is the hierarchy too shallow?
        if (node->isAtRootLevel())
        {
            return 1;
        }

        // So far so good. Move one level up the hierarchy.
        node  = &node->parent();
        snode = &searchPattern.reverseSegment(i + 1);
    }

    return 1;
}

#ifdef DE_STACK_MONITOR
static void *stackStart;
static size_t maxStackDepth;
#endif

namespace internal {
    struct PathConstructorArgs
    {
        dsize length;
        Char separator;
        String composedPath;

        PathConstructorArgs(Char sep = '/') : length(0), separator(sep)
        {}
    };
}

/**
 * Recursive path constructor. First finds the root and the full length of the
 * path (when descending), then allocates memory for the string, and finally
 * copies each segment with the separators (on the way out).
 */
static void pathConstructor(internal::PathConstructorArgs &args, const PathTree::Node &trav)
{
    const String &segment = trav.name();

#ifdef DE_STACK_MONITOR
    maxStackDepth = MAX_OF(maxStackDepth, stackStart - (void *)&fragment);
#endif

    args.length += segment.size();

    if (!trav.isAtRootLevel())
    {
        if (args.separator)
        {
            // There also needs to be a separator (a single character).
            args.length += 1;
        }

        // Descend to parent level.
        pathConstructor(args, trav.parent());

        // Append the separator.
        if (args.separator)
        {
            args.composedPath.append(args.separator);
        }
    }
    // We've arrived at the deepest level. The full length is now known.
    // Ensure there's enough memory for the string.
    else if (args.composedPath)
    {
        //args.composedPath.reserve(args.length);
    }

    // Assemble the path by appending the segment.
    args.composedPath.append(segment);
}

/**
 * @todo Paths within the tree store Path-compatible segment hashes. It would
 * be better to compose the full path in such a way that applies the already
 * calculated hashes. This would avoid the re-hash when the resultant path is
 * next compared (which is likely the reason the user is calling) -ds
 *
 * @todo This is a good candidate for result caching: the constructed path
 * could be saved and returned on subsequent calls. Are there any circumstances
 * in which the cached result becomes obsolete? -jk
 *
 * The only times the result becomes obsolete is when the delimiter is changed
 * or when the directory itself is rebuilt (in which case the nodes themselves
 * will be free'd). Note that any caching mechanism should not counteract one
 * of the primary goals of this class, i.e., optimal memory usage for the whole
 * directory. Caching constructed paths for every leaf node in the directory
 * would completely negate the benefits of the design of this class.
 *
 * Perhaps a fixed size MRU cache? -ds
 */
Path PathTree::Node::path(Char sep) const
{
    internal::PathConstructorArgs args(sep);
#ifdef DE_STACK_MONITOR
    stackStart = &parm;
#endif

    // Include a terminating path separator for branches.
    if (sep && isBranch())
    {
        args.length++; // A single character.
    }

    // Recursively construct the path from segments and separators.
    pathConstructor(args, *this);

    // Add a closing separator for branches.
    if (sep && isBranch())
    {
        args.composedPath += sep;
    }

    DE_ASSERT(args.composedPath.size() == args.length);

#ifdef DE_STACK_MONITOR
    LOG_AS("pathConstructor");
    LOG_DEV_NOTE("Max stack depth: %i bytes") << maxStackDepth;
#endif

    return Path(args.composedPath, sep);
}

UserDataNode::UserDataNode(const PathTree::NodeArgs &args, void *userPointer, int userValue)
    : Node(args)
    , _pointer(userPointer)
    , _value(userValue)
{}

void *UserDataNode::userPointer() const
{
    return _pointer;
}

int UserDataNode::userValue() const
{
    return _value;
}

UserDataNode &UserDataNode::setUserPointer(void *ptr)
{
    _pointer = ptr;
    return *this;
}

UserDataNode &UserDataNode::setUserValue(int value)
{
    _value = value;
    return *this;
}

} // namespace de
