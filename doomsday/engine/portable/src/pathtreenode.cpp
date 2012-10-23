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

struct de::PathTreeNode::Instance
{
    de::PathTreeNode* self;

    /// PathTree which owns this node.
    PathTree& tree;

    /// Symbolic node type.
    PathTreeNodeType type;

    /// Unique identifier for the path fragment this node represents,
    /// in the owning PathTree.
    StringPoolId internId;

    /// Parent node in the user's logical hierarchy.
    PathTreeNode* parent;

    /// User data pointer associated with this node.
    void* userData;

    Instance(de::PathTreeNode* d, de::PathTree& _tree, PathTreeNodeType _type,
             StringPoolId _internId, de::PathTreeNode* _parent)
        : self(d), tree(_tree), type(_type), internId(_internId), parent(_parent),
          userData(0)
    {}
};

de::PathTreeNode::PathTreeNode(PathTree& directory,
    PathTreeNodeType type, StringPoolId internId, de::PathTreeNode* parent,
    void* userData)
{
    d = new Instance(this, directory, type, internId, parent);
    setUserData(userData);
}

de::PathTreeNode::~PathTreeNode()
{
    delete d;
}

Str const* de::PathTreeNode::typeName(PathTreeNodeType type)
{
    static de::Str const nodeNames[1+PATHTREENODE_TYPE_COUNT] = {
        "(invalidtype)",
        "branch",
        "leaf"
    };
    if(!VALID_PATHTREENODE_TYPE(type)) return nodeNames[0];
    return nodeNames[1 + (type - PATHTREENODE_TYPE_FIRST)];
}

/// @return  PathTree which owns this node.
de::PathTree& de::PathTreeNode::tree() const
{
    return d->tree;
}

/// @return  Parent of this directory node else @c NULL
de::PathTreeNode* de::PathTreeNode::parent() const
{
    return d->parent;
}

/// @return  Type of this directory node.
PathTreeNodeType de::PathTreeNode::type() const
{
    return d->type;
}

StringPoolId de::PathTreeNode::internId() const
{
    return d->internId;
}

ushort de::PathTreeNode::hash() const
{
    return d->tree.hashForInternId(d->internId);
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
                st--;
            if(st < pattern)
                return false; // No match!
            // The asterisk lets us continue.
        }

        // This character of the pattern is OK.
        st++;
        in++;
    }

    // Match is good if the end of the pattern was reached.
    while(*st == '*')
        st++; // Skip remaining asterisks.

    return *st == 0;
}

/// @note This routine is also used as an iteration callback, so only return
///       a non-zero value when the node is a match for the search term.
int de::PathTreeNode::comparePath(int flags, PathMap* searchPattern) const
{
    if(((flags & PCF_NO_LEAF)   && PT_LEAF   == type()) ||
       ((flags & PCF_NO_BRANCH) && PT_BRANCH == type()))
        return false;

    PathMapFragment const* sfragment = PathMap_Fragment(searchPattern, 0);
    if(!sfragment) return false; // Hmm...

//#ifdef _DEBUG
//#  define EXIT_POINT(ep) fprintf(stderr, "MatchDirectory exit point %i\n", ep)
//#else
#  define EXIT_POINT(ep)
//#endif

    // In reverse order, compare path fragments in the search term.
    PathTree& pt = tree();
    uint fragmentCount = PathMap_Size(searchPattern);

    de::PathTreeNode const* node = this;
    for(uint i = 0; i < fragmentCount; ++i)
    {
        if(i == 0 && node->type() == PT_LEAF)
        {
            char buf[256];
            qsnprintf(buf, 256, "%*s", sfragment->to - sfragment->from + 1, sfragment->from);

            ddstring_t const* fragment = pt.pathFragment(*node);
            DENG2_ASSERT(fragment);

            if(!matchPathFragment(Str_Text(fragment), buf))
            {
                EXIT_POINT(1);
                return false;
            }
        }
        else
        {
            bool const isWild = (sfragment->to == sfragment->from && *sfragment->from == '*');
            if(!isWild)
            {
                // If the hashes don't match it can't possibly be this.
                if(sfragment->hash != pt.hashForInternId(node->internId()))
                {
                    EXIT_POINT(2);
                    return false;
                }

                // Determine length of the sfragment.
                int sfraglen;
                if(!strcmp(sfragment->to, "") && !strcmp(sfragment->from, ""))
                    sfraglen = 0;
                else
                    sfraglen = (sfragment->to - sfragment->from) + 1;

                // Compare the path fragment to that of the search term.
                ddstring_t const* fragment = pt.pathFragment(*node);
                if(Str_Length(fragment) < sfraglen ||
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
        sfragment = PathMap_Fragment(searchPattern, i+1);
    }
    EXIT_POINT(6);
    return false;

#undef EXIT_POINT
}

ddstring_t const* de::PathTreeNode::pathFragment() const
{
    return d->tree.pathFragment(*this);
}

ddstring_t* de::PathTreeNode::composePath(ddstring_t* path, int* length,
    char delimiter) const
{
    return d->tree.composePath(*this, path, length, delimiter);
}

void* de::PathTreeNode::userData() const
{
    return d->userData;
}

de::PathTreeNode& de::PathTreeNode::setUserData(void* userData)
{
    d->userData = userData;
    return *this;
}

/**
 * C wrapper API
 */

#define TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<de::PathTreeNode*>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<de::PathTreeNode const*>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    de::PathTreeNode* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    de::PathTreeNode const* self = TOINTERNAL_CONST(inst)

PathTree* PathTreeNode_Tree(PathTreeNode const* node)
{
    SELF_CONST(node);
    return reinterpret_cast<PathTree*>(&self->tree());
}

PathTreeNode* PathTreeNode_Parent(PathTreeNode const* node)
{
    SELF_CONST(node);
    return reinterpret_cast<PathTreeNode*>(self->parent());
}

PathTreeNodeType PathTreeNode_Type(PathTreeNode const* node)
{
    SELF_CONST(node);
    return self->type();
}

StringPoolId PathTreeNode_InternId(PathTreeNode const* node)
{
    SELF_CONST(node);
    return self->internId();
}

ushort PathTreeNode_Hash(PathTreeNode const* node)
{
    SELF_CONST(node);
    return self->hash();
}

int PathTreeNode_ComparePath(PathTreeNode* node, int flags,
    PathMap* searchPattern, void* /*parameters*/)
{
    SELF(node);
    return self->comparePath(flags, searchPattern);
}

ddstring_t* PathTreeNode_ComposePath2(PathTreeNode const* node,
    ddstring_t* path, int* length, char delimiter)
{
    SELF_CONST(node);
    return self->composePath(path, length, delimiter);
}

ddstring_t* PathTreeNode_ComposePath(PathTreeNode const* node,
    ddstring_t* path, int* length)
{
    SELF_CONST(node);
    return self->composePath(path, length);
}

ddstring_t const* PathTreeNode_PathFragment(PathTreeNode const* node)
{
    SELF_CONST(node);
    return self->pathFragment();
}

void* PathTreeNode_UserData(PathTreeNode const* node)
{
    SELF_CONST(node);
    return self->userData();
}

void PathTreeNode_SetUserData(PathTreeNode* node, void* userData)
{
    SELF(node);
    self->setUserData(userData);
}

Str const* PathTreeNodeType_Name(PathTreeNodeType type)
{
    return de::PathTreeNode::typeName(type);
}
