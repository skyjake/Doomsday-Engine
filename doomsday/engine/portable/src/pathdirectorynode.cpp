/**
 * @file pathdirectorynode.cpp
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
#include "pathdirectory.h"

struct de::PathDirectoryNode::Instance
{
    de::PathDirectoryNode* self;

    /// PathDirectory which owns this node.
    PathDirectory& directory;

    /// Symbolic node type.
    PathDirectoryNodeType type;

    /// Unique identifier for the path fragment this node represents,
    /// in the owning PathDirectory.
    StringPoolId internId;

    /// Parent node in the user's logical hierarchy.
    PathDirectoryNode* parent;

    /// User data pointer associated with this node.
    void* userData;

    Instance(de::PathDirectoryNode* d, de::PathDirectory& _directory,
             PathDirectoryNodeType _type, StringPoolId _internId,
             de::PathDirectoryNode* _parent)
        : self(d), directory(_directory), type(_type), internId(_internId), parent(_parent),
          userData(0)
    {}
};

de::PathDirectoryNode::PathDirectoryNode(PathDirectory& directory,
    PathDirectoryNodeType type, StringPoolId internId, de::PathDirectoryNode* parent,
    void* userData)
{
    d = new Instance(this, directory, type, internId, parent);
    setUserData(userData);
}

de::PathDirectoryNode::~PathDirectoryNode()
{
    delete d;
}

const Str* de::PathDirectoryNode::typeName(PathDirectoryNodeType type)
{
    static const de::Str nodeNames[1+PATHDIRECTORYNODE_TYPE_COUNT] = {
        "(invalidtype)",
        "branch",
        "leaf"
    };
    if(!VALID_PATHDIRECTORYNODE_TYPE(type)) return nodeNames[0];
    return nodeNames[1 + (type - PATHDIRECTORYNODE_TYPE_FIRST)];
}

/// @return  PathDirectory which owns this node.
de::PathDirectory& de::PathDirectoryNode::directory() const
{
    return d->directory;
}

/// @return  Parent of this directory node else @c NULL
de::PathDirectoryNode* de::PathDirectoryNode::parent() const
{
    return d->parent;
}

/// @return  Type of this directory node.
PathDirectoryNodeType de::PathDirectoryNode::type() const
{
    return d->type;
}

StringPoolId de::PathDirectoryNode::internId() const
{
    return d->internId;
}

ushort de::PathDirectoryNode::hash() const
{
    return d->directory.hashForInternId(d->internId);
}

static int matchPathFragment(const char* string, const char* pattern)
{
    const char* in = string, *st = pattern;

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
int de::PathDirectoryNode::matchDirectory(int flags, PathMap* searchPattern)
{
    if(((flags & PCF_NO_LEAF)   && PT_LEAF   == type()) ||
       ((flags & PCF_NO_BRANCH) && PT_BRANCH == type()))
        return false;

    const PathMapFragment* sfragment = PathMap_Fragment(searchPattern, 0);
    if(!sfragment) return false; // Hmm...

//#ifdef _DEBUG
//#  define EXIT_POINT(ep) fprintf(stderr, "MatchDirectory exit point %i\n", ep)
//#else
#  define EXIT_POINT(ep)
//#endif

    // In reverse order, compare path fragments in the search term.
    PathDirectory& pd = directory();
    uint fragmentCount = PathMap_Size(searchPattern);

    de::PathDirectoryNode* node = this;
    for(uint i = 0; i < fragmentCount; ++i)
    {
        if(i == 0 && node->type() == PT_LEAF)
        {
            char buf[256];
            qsnprintf(buf, 256, "%*s", sfragment->to - sfragment->from + 1, sfragment->from);

            const ddstring_t* fragment = pd.pathFragment(node);
            DENG2_ASSERT(fragment);

            if(!matchPathFragment(Str_Text(fragment), buf))
            {
                EXIT_POINT(1);
                return false;
            }
        }
        else
        {
            const bool isWild = (sfragment->to == sfragment->from && *sfragment->from == '*');
            if(!isWild)
            {
                // If the hashes don't match it can't possibly be this.
                if(sfragment->hash != pd.hashForInternId(node->internId()))
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
                const ddstring_t* fragment = pd.pathFragment(node);
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

const ddstring_t* de::PathDirectoryNode::pathFragment() const
{
    return d->directory.pathFragment(this);
}

ddstring_t* de::PathDirectoryNode::composePath(ddstring_t* path, int* length,
    char delimiter) const
{
    return d->directory.composePath(this, path, length, delimiter);
}

void* de::PathDirectoryNode::userData() const
{
    return d->userData;
}

de::PathDirectoryNode& de::PathDirectoryNode::setUserData(void* userData)
{
    d->userData = userData;
    return *this;
}

/**
 * C wrapper API
 */

#define TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<de::PathDirectoryNode*>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<const de::PathDirectoryNode*>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    de::PathDirectoryNode* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    const de::PathDirectoryNode* self = TOINTERNAL_CONST(inst)

PathDirectory* PathDirectoryNode_Directory(const PathDirectoryNode* node)
{
    SELF_CONST(node);
    return reinterpret_cast<PathDirectory*>(&self->directory());
}

PathDirectoryNode* PathDirectoryNode_Parent(const PathDirectoryNode* node)
{
    SELF_CONST(node);
    return reinterpret_cast<PathDirectoryNode*>(self->parent());
}

PathDirectoryNodeType PathDirectoryNode_Type(const PathDirectoryNode* node)
{
    SELF_CONST(node);
    return self->type();
}

StringPoolId PathDirectoryNode_InternId(const PathDirectoryNode* node)
{
    SELF_CONST(node);
    return self->internId();
}

ushort PathDirectoryNode_Hash(const PathDirectoryNode* node)
{
    SELF_CONST(node);
    return self->hash();
}

int PathDirectoryNode_MatchDirectory(PathDirectoryNode* node, int flags,
    PathMap* searchPattern, void* /*parameters*/)
{
    SELF(node);
    return self->matchDirectory(flags, searchPattern);
}

ddstring_t* PathDirectoryNode_ComposePath2(const PathDirectoryNode* node,
    ddstring_t* path, int* length, char delimiter)
{
    SELF_CONST(node);
    return self->composePath(path, length, delimiter);
}

ddstring_t* PathDirectoryNode_ComposePath(const PathDirectoryNode* node,
    ddstring_t* path, int* length)
{
    SELF_CONST(node);
    return self->composePath(path, length);
}

const ddstring_t* PathDirectoryNode_PathFragment(const PathDirectoryNode* node)
{
    SELF_CONST(node);
    return self->pathFragment();
}

void* PathDirectoryNode_UserData(const PathDirectoryNode* node)
{
    SELF_CONST(node);
    return self->userData();
}

void PathDirectoryNode_SetUserData(PathDirectoryNode* node, void* userData)
{
    SELF(node);
    self->setUserData(userData);
}

const Str* PathDirectoryNodeType_Name(PathDirectoryNodeType type)
{
    return de::PathDirectoryNode::typeName(type);
}
