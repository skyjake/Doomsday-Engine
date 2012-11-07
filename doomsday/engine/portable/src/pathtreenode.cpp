/**
 * @file pathtreenode.cpp
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

#include <de/Log>
#include "pathtree.h"

namespace de {

struct PathTree::Node::Instance
{
    /// @c true = this is a leaf node.
    bool isLeaf;

    /// PathTree which owns this node.
    PathTree& tree;

    /// Unique identifier for the path fragment this node represents,
    /// in the owning PathTree.
    PathTree::FragmentId fragmentId;

    /// Parent node in the user's logical hierarchy.
    PathTree::Node* parent;

    /// User-specified data pointer associated with this node.
    void* userPointer;

    /// User-specified value associated with this node.
    int userValue;

    Instance(PathTree& _tree, bool _isLeaf, PathTree::FragmentId _fragmentId,
             PathTree::Node* _parent)
        : isLeaf(_isLeaf), tree(_tree), fragmentId(_fragmentId), parent(_parent),
          userPointer(0), userValue(0)
    {}
};

PathTree::Node::Node(PathTree& tree, PathTree::NodeType type, PathTree::FragmentId fragmentId,
    PathTree::Node* parent, void* userPointer, int userValue)
{
    d = new Instance(tree, type == PathTree::Leaf, fragmentId, parent);
    setUserPointer(userPointer);
    setUserValue(userValue);
}

PathTree::Node::~Node()
{
    delete d;
}

bool PathTree::Node::isLeaf() const
{
    return d->isLeaf;
}

PathTree& PathTree::Node::tree() const
{
    return d->tree;
}

PathTree::Node* PathTree::Node::parent() const
{
    return d->parent;
}

PathTree::FragmentId PathTree::Node::fragmentId() const
{
    return d->fragmentId;
}

ddstring_t const* PathTree::Node::name() const
{
    return tree().fragmentName(d->fragmentId);
}

ushort PathTree::Node::hash() const
{
    return tree().fragmentHash(d->fragmentId);
}

void* PathTree::Node::userPointer() const
{
    return d->userPointer;
}

int PathTree::Node::userValue() const
{
    return d->userValue;
}

PathTree::Node& PathTree::Node::setUserPointer(void* ptr)
{
    d->userPointer = ptr;
    return *this;
}

PathTree::Node& PathTree::Node::setUserValue(int value)
{
    d->userValue = value;
    return *this;
}

static int matchPathFragment(char const* string, char const* pattern)
{
    char const* in = string, *st = pattern;

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

/// @note This routine is also used as an iteration callback, so only return
///       a non-zero value when the node is a match for the search term.
int PathTree::Node::comparePath(PathMap& searchPattern, int flags) const
{
    if(((flags & PCF_NO_LEAF)   && isLeaf()) ||
       ((flags & PCF_NO_BRANCH) && !isLeaf()))
        return false;

    PathMapFragment const* sfragment = PathMap_Fragment(&searchPattern, 0);
    if(!sfragment) return false; // Hmm...

//#ifdef _DEBUG
//#  define EXIT_POINT(ep) fprintf(stderr, "MatchDirectory exit point %i\n", ep)
//#else
#  define EXIT_POINT(ep)
//#endif

    // In reverse order, compare path fragments in the search term.
    uint fragmentCount = PathMap_Size(&searchPattern);

    PathTree::Node const* node = this;
    for(uint i = 0; i < fragmentCount; ++i)
    {
        if(i == 0 && node->isLeaf())
        {
            char buf[256];
            qsnprintf(buf, 256, "%*s", sfragment->to - sfragment->from + 1, sfragment->from);

            ddstring_t const* fragment = node->name();
            if(!matchPathFragment(Str_Text(fragment), buf))
            {
                EXIT_POINT(1);
                return false;
            }
        }
        else
        {
            if(!sfragment->isWild())
            {
                // If the hashes don't match it can't possibly be this.
                if(sfragment->hash != node->hash())
                {
                    EXIT_POINT(2);
                    return false;
                }

                // Compare the path fragment to that of the search term.
                ddstring_t const* fragment = node->name();
                if(Str_Length(fragment) < sfragment->length() ||
                   qstrnicmp(Str_Text(fragment), sfragment->from, Str_Length(fragment)))
                {
                    EXIT_POINT(3);
                    return false;
                }
            }
        }

        // Have we arrived at the search target?
        if(i == fragmentCount-1)
        {
            EXIT_POINT(4);
            return (!(flags & PCF_MATCH_FULL) || !node->parent());
        }

        // Are there no more parent directories?
        if(!node->parent())
        {
            EXIT_POINT(5);
            return false;
        }

        // So far so good. Move one directory level upwards.
        node = node->parent();
        sfragment = PathMap_Fragment(&searchPattern, i + 1);
    }
    EXIT_POINT(6);
    return false;

#undef EXIT_POINT
}

#ifdef LIBDENG_STACK_MONITOR
static void* stackStart;
static size_t maxStackDepth;
#endif

typedef struct pathconstructorparams_s {
    size_t length;
    ddstring_t* composedPath;
    char delimiter;
} pathconstructorparams_t;

/**
 * Recursive path constructor. First finds the root and the full length of the
 * path (when descending), then allocates memory for the string, and finally
 * copies each fragment with the delimiters (on the way out).
 */
static void pathConstructor(pathconstructorparams_t& parm, PathTree::Node const& trav)
{
    ddstring_t const* fragment = trav.name();

#ifdef LIBDENG_STACK_MONITOR
    maxStackDepth = MAX_OF(maxStackDepth, stackStart - (void*)&fragment);
#endif

    parm.length += Str_Length(fragment);

    if(trav.parent())
    {
        if(parm.delimiter)
        {
            // There also needs to be a delimiter (a single character).
            parm.length += 1;
        }

        // Descend to parent level.
        pathConstructor(parm, *trav.parent());

        // Append the separator.
        if(parm.composedPath && parm.delimiter)
            Str_AppendCharWithoutAllocs(parm.composedPath, parm.delimiter);
    }
    // We've arrived at the deepest level. The full length is now known.
    // Ensure there's enough memory for the string.
    else if(parm.composedPath)
    {
        Str_ReserveNotPreserving(parm.composedPath, parm.length);
    }

    if(parm.composedPath)
    {
        // Assemble the path by appending the fragment.
        Str_AppendWithoutAllocs(parm.composedPath, fragment);
    }
}

/**
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
ddstring_t* PathTree::Node::composePath(ddstring_t* path, int* length, char delimiter) const
{
    pathconstructorparams_t parm;

#ifdef LIBDENG_STACK_MONITOR
    stackStart = &parm;
#endif

    parm.composedPath = path;
    parm.length = 0;
    parm.delimiter = delimiter;

    // Include a terminating path delimiter for branches.
    if(delimiter && !isLeaf())
        parm.length += 1; // A single character.

    if(parm.composedPath)
    {
        Str_Clear(parm.composedPath);
    }

    // Recursively construct the path from fragments and delimiters.
    pathConstructor(parm, *this);

    // Terminating delimiter for branches.
    if(parm.composedPath && delimiter && !isLeaf())
        Str_AppendCharWithoutAllocs(parm.composedPath, delimiter);

    DENG2_ASSERT(!parm.composedPath || Str_Size(parm.composedPath) == parm.length);

#ifdef LIBDENG_STACK_MONITOR
    LOG_AS("pathConstructor");
    LOG_INFO("Max stack depth: %1 bytes") << maxStackDepth;
#endif

    if(length) *length = parm.length;
    return path;
}

} // namespace de
