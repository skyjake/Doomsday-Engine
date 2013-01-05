/** @file pathtreenode.cpp PathTree::Node implementation.
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/PathTree"
#include "de/Log"

namespace de {

struct PathTree::Node::Instance
{
    /// PathTree which owns this node.
    PathTree &tree;

    /// Parent node in the user's logical hierarchy.
    PathTree::Node *parent;

    /// @c NULL for leaves, index of children for branches.
    PathTree::Node::Children *children;

    /// Unique identifier for the path fragment this node represents,
    /// in the owning PathTree.
    PathTree::SegmentId segmentId;

    Instance(PathTree &_tree, bool isLeaf, PathTree::SegmentId _segmentId,
             PathTree::Node *_parent)
        : tree(_tree), parent(_parent), children(0), segmentId(_segmentId)
    {
        if(!isLeaf) children = new PathTree::Node::Children;
    }

    ~Instance()
    {
        delete children;
    }
};

PathTree::Node::Node(PathTree::NodeArgs const &args)
{
    d = new Instance(args.tree, args.type == PathTree::Leaf, args.segmentId, args.parent);

    // Let the parent know of the new child node.
    if(d->parent) d->parent->addChild(*this);
}

PathTree::Node::~Node()
{
    delete d;
}

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
    DENG2_ASSERT(d->children != 0);
    return *d->children;
}

PathTree::Nodes const &PathTree::Node::childNodes(PathTree::NodeType type) const
{
    DENG2_ASSERT(d->children != 0);
    return (type == PathTree::Leaf? d->children->leaves : d->children->branches);
}

PathTree::Nodes &PathTree::Node::childNodes(PathTree::NodeType type)
{
    DENG2_ASSERT(d->children != 0);
    return (type == PathTree::Leaf? d->children->leaves : d->children->branches);
}

bool PathTree::Node::isAtRootLevel() const
{
    return d->parent == &d->tree.rootBranch();
}

PathTree::SegmentId PathTree::Node::segmentId() const
{
    return d->segmentId;
}

void PathTree::Node::addChild(PathTree::Node &node)
{
    DENG2_ASSERT(d->children != 0);

    childNodes(node.type()).insert(node.hash(), &node);
}

void PathTree::Node::removeChild(PathTree::Node &node)
{
    DENG2_ASSERT(d->children != 0);

    childNodes(node.type()).remove(node.hash(), &node);
}

String const &PathTree::Node::name() const
{
    return tree().segmentName(d->segmentId);
}

Path::hash_type PathTree::Node::hash() const
{
    return tree().segmentHash(d->segmentId);
}

/// @todo This logic should be encapsulated in de::Path or de::Path::Segment; use QChar.
static int matchName(char const *string, char const *pattern)
{
    char const *in = string, *st = pattern;

    while(*in)
    {
        if(*st == '*')
        {
            st++;
            continue;
        }

        if(*st != '?' && (tolower((unsigned char) *st) != tolower((unsigned char) *in)))
        {
            // A mismatch. Hmm. Go back to a previous '*'.
            while(st >= pattern && *st != '*')
            { st--; }

            // No match?
            if(st < pattern) return false;

            // The asterisk lets us continue.
        }

        // This character of the pattern is OK.
        st++;
        in++;
    }

    // Skip remaining asterisks.
    while(*st == '*')
    { st++; }

    // Match is good if the end of the pattern was reached.
    return *st == 0;
}

int PathTree::Node::comparePath(de::Path const &searchPattern, ComparisonFlags flags) const
{
    if(((flags & PathTree::NoLeaf)   && isLeaf()) ||
       ((flags & PathTree::NoBranch) && isBranch()))
        return 1;

    try
    {
        de::Path::Segment const *snode = &searchPattern.lastSegment();

        // In reverse order, compare each path node in the search term.
        int pathNodeCount = searchPattern.segmentCount();

        PathTree::Node const *node = this;
        for(int i = 0; i < pathNodeCount; ++i)
        {
            bool const snameIsWild = !snode->toString().compare("*");
            if(!snameIsWild)
            {
                // If the hashes don't match it can't possibly be this.
                if(snode->hash() != node->hash())
                {
                    return 1;
                }

                // Compare the names.
                /// @todo Optimize: conversion to string is unnecessary.
                QByteArray name  = node->name().toUtf8();
                QByteArray sname = snode->toString().toUtf8();

                if(!matchName(name.constData(), sname.constData()))
                {
                    return 1;
                }
            }

            // Have we arrived at the search target?
            if(i == pathNodeCount - 1)
            {
                return !(!(flags & MatchFull) || node->isAtRootLevel());
            }

            // Is the hierarchy too shallow?
            if(node->isAtRootLevel())
            {
                return 1;
            }

            // So far so good. Move one level up the hierarchy.
            node  = &node->parent();
            snode = &searchPattern.reverseSegment(i + 1);
        }
    }
    catch(de::Path::OutOfBoundsError const &)
    {} // Ignore this error.

    return 1;
}

#ifdef LIBDENG_STACK_MONITOR
static void *stackStart;
static size_t maxStackDepth;
#endif

namespace internal {
    struct PathConstructorArgs
    {
        int length;
        QChar separator;
        String composedPath;

        PathConstructorArgs(QChar sep = '/') : length(0), separator(sep)
        {}
    };
}

/**
 * Recursive path constructor. First finds the root and the full length of the
 * path (when descending), then allocates memory for the string, and finally
 * copies each segment with the separators (on the way out).
 */
static void pathConstructor(internal::PathConstructorArgs &args, PathTree::Node const &trav)
{
    String const &segment = trav.name();

#ifdef LIBDENG_STACK_MONITOR
    maxStackDepth = MAX_OF(maxStackDepth, stackStart - (void *)&fragment);
#endif

    args.length += segment.length();

    if(!trav.isAtRootLevel())
    {
        if(!args.separator.isNull())
        {
            // There also needs to be a separator (a single character).
            args.length += 1;
        }

        // Descend to parent level.
        pathConstructor(args, trav.parent());

        // Append the separator.
        if(!args.separator.isNull())
            args.composedPath.append(args.separator);
    }
    // We've arrived at the deepest level. The full length is now known.
    // Ensure there's enough memory for the string.
    else if(args.composedPath)
    {
        args.composedPath.reserve(args.length);
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
Path PathTree::Node::path(QChar sep) const
{
    internal::PathConstructorArgs args(sep);
#ifdef LIBDENG_STACK_MONITOR
    stackStart = &parm;
#endif

    // Include a terminating path separator for branches.
    if(!sep.isNull() && isBranch())
    {
        args.length++; // A single character.
    }

    // Recursively construct the path from segments and separators.
    pathConstructor(args, *this);

    // Add a closing separator for branches.
    if(!sep.isNull() && isBranch())
    {
        args.composedPath += sep;
    }

    DENG2_ASSERT(args.composedPath.length() == (int)args.length);

#ifdef LIBDENG_STACK_MONITOR
    LOG_AS("pathConstructor");
    LOG_INFO("Max stack depth: %1 bytes") << maxStackDepth;
#endif

    return Path(args.composedPath, sep);
}

UserDataNode::UserDataNode(PathTree::NodeArgs const &args, void *userPointer, int userValue)
    : PathTree::Node(args),
      _pointer(userPointer),
      _value(userValue)
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
