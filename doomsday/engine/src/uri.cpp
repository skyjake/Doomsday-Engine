/**
 * @file uri.cpp
 * Universal Resource Identifier. @ingroup base
 *
 * @author Copyright &copy; 2010-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2010-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "uri.hh"
#include <de/str.h>
#include <de/unittest.h>
#include <de/NativePath>
#include <QDebug>

#include "de_base.h"
#include "dualstring.h"
#include "game.h"
#include "resource/resourcenamespace.h"
#include "resource/sys_reslocator.h"

/// Size of the fixed-size path node buffer.
#define PATHNODEBUFFER_SIZE         24

namespace de {

Uri::PathNode* Uri::PathNode::parent() const
{
    return parent_;
}

ushort Uri::PathNode::hash()
{
    // Is it time to compute the hash?
    if(!haveHashKey)
    {
        String str = String(from, length());
        hashKey = 0;
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
        hashKey %= hash_range;
        haveHashKey = true;
    }
    return hashKey;
}

int Uri::PathNode::length() const
{
    if(!qstrcmp(to, "") && !qstrcmp(from, ""))
        return 0;
    return (to - from) + 1;
}

String Uri::PathNode::toString() const
{
    return String(from, length());
}

struct Uri::Instance
{
    /// Total number of nodes in the path.
    int pathNodeCount;

    /// Head of the linked list of "extra" path nodes, in reverse order.
    Uri::PathNode* extraPathNodes;

    /**
     * Node map of the path. The map is composed of two components; the first
     * PATHNODEBUFFER_SIZE elements are placed into a fixed-size buffer which
     * is allocated along with *this* instance. Additional nodes are allocated
     * dynamically and linked in the @ref extraPathNodes list.
     *
     * This optimized representation should mean that the majority of paths
     * can be represented without dynamically allocating memory from the heap.
     */
    Uri::PathNode pathNodeBuffer[PATHNODEBUFFER_SIZE];

    /// Cached copy of the resolved Uri.
    String resolved;

    /// The cached copy only applies when this game is loaded. Add any other
    /// conditions here that result in different results for resolveUri().
    void* resolvedForGame;

    DualString scheme;
    DualString path;

    Instance()
        : pathNodeCount(-1), extraPathNodes(0), resolved(), resolvedForGame(0)
    {
        //Str_InitStd(&scheme);
        //Str_InitStd(&path);
        memset(pathNodeBuffer, 0, sizeof(pathNodeBuffer));
    }

    ~Instance()
    {
        clearPathMap();
        //Str_Free(&scheme);
        //Str_Free(&path);
    }

    /// @return  @c true iff @a node comes from the "static" buffer.
    inline bool isStaticPathNode(Uri::PathNode const& node)
    {
        return &node >= pathNodeBuffer &&
               &node < (pathNodeBuffer + sizeof(*pathNodeBuffer) * PATHNODEBUFFER_SIZE);
    }

    /**
     * Return the path name map to an empty state.
     *
     * @post The map will need to be rebuilt with @ref mapPath().
     */
    void clearPathMap()
    {
        while(extraPathNodes)
        {
            Uri::PathNode* next = extraPathNodes->parent();
            delete extraPathNodes; extraPathNodes = next;
        }
        memset(pathNodeBuffer, 0, sizeof(pathNodeBuffer));
        pathNodeCount = -1;
    }

    /**
     * Allocate another path node, from either the "static" buffer if one is
     * available or dynamically from the heap.
     *
     * @see isStaticPathNode() to determine if the node is "static".
     *
     * @return  New path node.
     */
    Uri::PathNode* allocPathNode(char const* from, char const* to)
    {
        Uri::PathNode* node;
        if(pathNodeCount < PATHNODEBUFFER_SIZE)
        {
            node = pathNodeBuffer + pathNodeCount;
            memset(node, 0, sizeof(*node));
        }
        else
        {
            // Allocate an "extra" node.
            node = new Uri::PathNode();
        }

        node->from = from;
        node->to   = to;

        return node;
    }

    /**
     * Build the path name map.
     */
    void mapPath()
    {
        // Already been here?
        if(pathNodeCount >= 0) return;

        pathNodeCount = 0;
        extraPathNodes = 0;

        //if(Str_IsEmpty(&path)) return;

        char const* nameBegin = path.utf8CStr();
        char const* nameEnd = nameBegin + path.length() - 1;

        // Skip over any trailing delimiters.
        for(int i = path.length(); *nameEnd && *nameEnd == '/' && i-- > 0; nameEnd--)
        {}

        // Scan the path hierarchy for node names, in reverse order.
        Uri::PathNode* node = 0;
        char const* from;
        forever
        {
            // Find the start of the next name.
            for(from = nameEnd; from > nameBegin && !(*from == '/'); from--)
            {}

            Uri::PathNode* newNode = allocPathNode(*from == '/'? from + 1 : from, nameEnd);

            // There is now one more node in the path map.
            pathNodeCount += 1;

            // "extra" nodes are linked to the tail of the extraPathNodes list.
            if(!isStaticPathNode(*newNode))
            {
                if(!extraPathNodes)
                {
                    extraPathNodes = newNode;
                }
                else
                {
                    node->parent_ = extraPathNodes;
                }
            }

            node = newNode;

            // Are there no more parent directories?
            if(from == nameBegin) break;

            // So far so good. Move one directory level upwards.
            // The next name ends here.
            nameEnd = from - 1;
        }

        // Deal with the special case of a Unix style zero-length root name.
        if(*nameBegin == '/')
        {
            Uri::PathNode* newNode = allocPathNode("", "");

            // There is now one more node in the path map.
            pathNodeCount += 1;

            // "extra" nodes are linked to the tail of the extraPathNodes list.
            if(!isStaticPathNode(*newNode))
            {
                if(!extraPathNodes)
                {
                    extraPathNodes = newNode;
                }
                else
                {
                    node->parent_ = extraPathNodes;
                }
            }
        }
    }

    void clearCachedResolved()
    {
        resolved.clear();
        resolvedForGame = 0;
    }

    void parseScheme(resourceclassid_t defaultResourceClass)
    {
        LOG_AS("Uri::parseScheme");

        const char* pathUtf8 = path.utf8CStr();

        clearCachedResolved();
        scheme.clear();

        char const* p = Str_CopyDelim2(scheme.toStr(), pathUtf8, ':', CDF_OMIT_DELIMITER);
        scheme.update();

        if(!p || p - pathUtf8 < URI_MINSCHEMELENGTH + 1) // +1 for ':' delimiter.
        {
            scheme.clear();
        }
        else if(defaultResourceClass != RC_NULL && !F_ResourceNamespaceByName(scheme.utf8CStr()))
        {
            LOG_WARNING("Unknown scheme in path \"%s\", using default.") << path;
            //Str_Clear(&_scheme);
            path = p;
        }
        else
        {
            path = p;
            return;
        }

        // Attempt to guess the scheme by interpreting the path?
        if(defaultResourceClass == RC_UNKNOWN)
        {
            defaultResourceClass = F_GuessResourceTypeFromFileName(path.utf8CStr()).defaultClass();
        }

        if(VALID_RESOURCE_CLASSID(defaultResourceClass))
        {
            ResourceNamespace* rnamespace = F_ResourceNamespaceByName(F_ResourceClassById(defaultResourceClass)->defaultNamespace());
            DENG_ASSERT(rnamespace);
            scheme = rnamespace->name();
        }
    }

    String resolveSymbol(QStringRef const& symbol)
    {
        if(!symbol.compare("App.DataPath", Qt::CaseInsensitive))
        {
            return "data";
        }
        else if(!symbol.compare("App.DefsPath", Qt::CaseInsensitive))
        {
            return "defs";
        }
        else if(!symbol.compare("Game.IdentityKey", Qt::CaseInsensitive))
        {
            de::Game& game = *reinterpret_cast<de::Game*>(App_CurrentGame());
            if(isNullGame(game))
            {
                /// @throw ResolveSymbolError  An unresolveable symbol was encountered.
                throw ResolveSymbolError("Uri::resolveSymbol", "Symbol 'Game' did not resolve (no game loaded)");
            }

            return Str_Text(&game.identityKey());
        }
        else if(!symbol.compare("GamePlugin.Name", Qt::CaseInsensitive))
        {
            if(!DD_GameLoaded() || !gx.GetVariable)
            {
                /// @throw ResolveSymbolError  An unresolveable symbol was encountered.
                throw ResolveSymbolError("Uri::resolveSymbol", "Symbol 'GamePlugin' did not resolve (no game plugin loaded)");
            }

            return String((char*)gx.GetVariable(DD_PLUGIN_NAME));
        }
        else
        {
            /// @throw UnknownSymbolError  An unknown symbol was encountered.
            throw UnknownSymbolError("Uri::resolveSymbol", "Symbol '" + symbol.toString() + "' is unknown");
        }
    }

    inline String parseExpression(QStringRef const& expression)
    {
        // Presently the expression consists of a single symbol.
        return resolveSymbol(expression);
    }

    void resolve(String& result)
    {
        LOG_AS("Uri::resolve");

        String const pathStr = path;

        // Keep scanning the path for embedded expressions.
        QStringRef expression;
        int expEnd = 0, expBegin;
        while((expBegin = pathStr.indexOf('$', expEnd)) >= 0)
        {
            // Is the next char the start-of-expression character?
            if(pathStr.at(expBegin + 1) == '(')
            {
                // Copy everything up to the '$'.
                result += pathStr.mid(expEnd, expBegin - expEnd);

                // Skip over the '$'.
                ++expBegin;

                // Find the end-of-expression character.
                expEnd = pathStr.indexOf(')', expBegin);
                if(expEnd < 0)
                {
                    LOG_WARNING("Missing closing ')' in expression \"" + pathStr + "\", ignoring.");
                    expEnd = pathStr.length();
                }

                // Skip over the '('.
                ++expBegin;

                // The range of the expression substring is now known.
                expression = pathStr.midRef(expBegin, expEnd - expBegin);

                result += parseExpression(expression);
            }
            else
            {
                // No - copy the '$' and continue.
                result += '$';
            }

            ++expEnd;
        }

        // Copy anything remaining.
        result += pathStr.mid(expEnd);
    }
};

Uri::Uri(String path, resourceclassid_t defaultResourceClass, QChar delimiter)
{
    d = new Instance();
    if(!path.isEmpty())
    {
        setUri(path, defaultResourceClass, delimiter);
    }
}

Uri::Uri()
{
    d = new Instance();
}

Uri::Uri(Uri const& other) : LogEntry::Arg::Base()
{
    d = new Instance();

    d->resolved        = other.d->resolved;
    d->resolvedForGame = other.d->resolvedForGame;
    d->scheme          = other.scheme();
    d->path            = other.path();
}

Uri Uri::fromNativePath(NativePath const& path)
{
    return Uri(path.expand().withSeparators('/'), RC_NULL);
}

Uri Uri::fromNativeDirPath(NativePath const& nativeDirPath, resourceclassid_t defaultResourceClass)
{
    // Uri follows the convention of having a slash at the end for directories.
    return Uri(nativeDirPath.expand().withSeparators('/') + '/', defaultResourceClass);
}

/*
Uri Uri::fromReader(struct reader_s& reader)
{
    return Uri().read(reader);
}*/

Uri::~Uri()
{
    delete d;
}

int Uri::pathNodeCount() const
{
    d->mapPath();
    return d->pathNodeCount;
}

Uri::PathNode& Uri::pathNode(int index) const
{
    d->mapPath();

    if(index < 0 || index >= d->pathNodeCount)
    {
        /// @throw NotPathNodeError  Attempt to reference a nonexistent fragment.
        throw NotPathNodeError("Uri::fragment", String("Index #%1 references a nonexistent fragment").arg(index));
    }

    // Is this in the static buffer?
    if(index < PATHNODEBUFFER_SIZE)
    {
        return d->pathNodeBuffer[index];
    }

    // No - an extra fragment.
    DENG_ASSERT(d->extraPathNodes);
    Uri::PathNode* fragment = d->extraPathNodes;
    int n = PATHNODEBUFFER_SIZE;
    while(n++ < index)
    {
        fragment = fragment->parent();
    }
    DENG_ASSERT(fragment);
    return *fragment;
}

Uri& Uri::operator = (Uri other)
{
    swap(*this, other);
    return *this;
}

bool Uri::operator == (Uri const& other) const
{
    if(this == &other) return true;

    // First, lets check if the scheme differs.
    if(d->scheme.length() != other.scheme().length()) return false;
    if(d->scheme.compareWithoutCase(other.scheme())) return false;

    // Is resolving not necessary?
    if(!d->path.compareWithoutCase(other.path())) return true;

    // We must be able to resolve both paths to compare.
    try
    {
        String const& thisPath = resolved();
        String const& otherPath = other.resolved();

        // Do not match partial paths.
        if(thisPath.length() != otherPath.length()) return false;

        return thisPath.compareWithoutCase(otherPath) == 0;
    }
    catch(ResolveError const&)
    {} // Ignore the error.
    return false;
}

bool Uri::operator != (Uri const& other) const
{
    return !(*this == other);
}

void swap(Uri& first, Uri& second)
{
    std::swap(first.d->pathNodeCount,   second.d->pathNodeCount);
    std::swap(first.d->extraPathNodes,  second.d->extraPathNodes);
    //std::swap(first.d->pathNodeBuffer,  second.d->pathNodeBuffer);
#ifdef DENG2_QT_4_8_OR_NEWER
    first.d->resolved.swap(second.d->resolved);
#else
    std::swap(first.d->resolved,        second.d->resolved);
#endif
    std::swap(first.d->resolvedForGame, second.d->resolvedForGame);
    /// @todo Is it valid to std::swap a ddstring_t ?
    std::swap(first.d->scheme,          second.d->scheme);
    std::swap(first.d->path,            second.d->path);
}

bool Uri::isEmpty() const
{
    return d->path.isEmpty();
}

Uri& Uri::clear()
{
    d->scheme.clear();
    d->path.clear();
    d->clearCachedResolved();
    d->clearPathMap();
    return *this;
}

String Uri::scheme() const
{
    return d->scheme;
}

String Uri::path() const
{
    return d->path;
}

const char* Uri::schemeCStr() const
{
    return d->scheme.utf8CStr();
}

const char* Uri::pathCStr() const
{
    return d->path.utf8CStr();
}

const ddstring_s* Uri::schemeStr() const
{
    return d->scheme.toStr();
}

const ddstring_s* Uri::pathStr() const
{
    return d->path.toStr();
}

String Uri::resolved() const
{
#ifndef LIBDENG_DISABLE_URI_RESOLVE_CACHING
    if(d->resolvedForGame && d->resolvedForGame == (void*) App_CurrentGame())
    {
        // We can just return the previously prepared resolved URI.
        return d->resolved;
    }
#endif

    d->clearCachedResolved();

    String result;
    d->resolve(result);

    // Keep a copy of this, we'll likely need it many, many times.
    d->resolved = result;
    d->resolvedForGame = (void*) App_CurrentGame();

    return d->resolved;
}

Uri& Uri::setScheme(String newScheme)
{
    d->scheme = newScheme;
    d->clearCachedResolved();
    return *this;
}

Uri& Uri::setPath(String newPath, QChar delimiter)
{
    if(delimiter != '/')
    {
        newPath = newPath.replace(delimiter, QString("/"), Qt::CaseInsensitive);
    }
    d->path = newPath;
    d->clearCachedResolved();
    d->clearPathMap();
    return *this;
}

Uri& Uri::setUri(String rawUri, resourceclassid_t defaultResourceClass, QChar delimiter)
{
    LOG_AS("Uri::setUri");

    if(delimiter != '/')
    {
        rawUri = rawUri.replace(delimiter, QString("/"), Qt::CaseInsensitive);
    }

    d->path = rawUri.trimmed();
    d->parseScheme(defaultResourceClass);
    d->clearCachedResolved();
    d->clearPathMap();
    return *this;
}

String Uri::compose(QChar delimiter) const
{
    String result;
    if(!d->scheme.isEmpty())
    {
        result += d->scheme + ":" + d->path;
    }
    else
    {
        result += d->path;
    }
    if(delimiter != '/')
    {
        result = result.replace('/', delimiter, Qt::CaseInsensitive);
    }
    return result;
}

String Uri::asText() const
{
    // Just compose it for now, we can worry about making it 'pretty' later.
    QByteArray composed = compose().toUtf8();
    AutoStr* path = AutoStr_FromTextStd(composed.constData());
    Str_PercentDecode(path);
    return String(Str_Text(path));
}

void Uri::debugPrint(int indent, PrintFlags flags, String unresolvedText) const
{
    indent = MAX_OF(0, indent);

    bool resolvedPath = (flags & OutputResolved) && !d->resolved.isEmpty();
    if(unresolvedText.isEmpty()) unresolvedText = "--(!)incomplete";

    LOG_DEBUG("%*s\"%s\"%s%s") << indent << ""
      << ((flags & TransformPathPrettify)? NativePath(asText()).pretty() : asText())
      << ((flags & OutputResolved)? (resolvedPath? "=> " : unresolvedText) : "")
      << ((flags & OutputResolved) && resolvedPath? ((flags & TransformPathPrettify)?
                                                      NativePath(d->resolved).pretty() : NativePath(d->resolved)) : "");
}

Uri::hash_type const Uri::hash_range = 512;

#if 0 //#ifdef _DEBUG
LIBDENG_DEFINE_UNITTEST(Uri)
{
    try
    {
        Uri::PathNode const* fragment;
        int len;

        // Test a zero-length path.
        {
        Uri u = Uri("", RC_NULL);
        DENG_ASSERT(u.pathNodeCount() == 0);
        }

        // Test a Windows style path with a drive plus file path.
        {
        Uri u = Uri("c:/something.ext", RC_NULL);
        DENG_ASSERT(u.pathNodeCount() == 2);

        fragment = u.pathNode(0);
        len = fragment->to - fragment->from;
        DENG_ASSERT(len == 12);
        DENG_ASSERT(!qstrncmp(fragment->from, "something.ext", len+1));

        fragment = u.pathNode(1);
        len = fragment->to - fragment->from;
        DENG_ASSERT(len == 1);
        DENG_ASSERT(!qstrncmp(fragment->from, "c:", len+1));
        }

        // Test a Unix style path with a zero-length root node name.
        {
        Uri u = Uri("/something.ext", RC_NULL);
        DENG_ASSERT(u.pathNodeCount() == 2);

        fragment = u.pathNode(0);
        len = fragment->to - fragment->from;
        DENG_ASSERT(len == 12);
        DENG_ASSERT(!qstrncmp(fragment->from, "something.ext", len+1));

        fragment = u.pathNode(1);
        len = fragment->to - fragment->from;
        DENG_ASSERT(len == 0);
        DENG_ASSERT(!qstrncmp(fragment->from, "", len));
        }

        // Test a relative directory.
        {
        Uri u = Uri("some/dir/structure/", RC_NULL);
        DENG_ASSERT(u.pathNodeCount() == 3);

        fragment = u.pathNode(0);
        len = fragment->to - fragment->from;
        DENG_ASSERT(len == 8);
        DENG_ASSERT(!qstrncmp(fragment->from, "structure", len+1));

        fragment = u.pathNode(1);
        len = fragment->to - fragment->from;
        DENG_ASSERT(len == 2);
        DENG_ASSERT(!qstrncmp(fragment->from, "dir", len+1));

        fragment = u.pathNode(2);
        len = fragment->to - fragment->from;
        DENG_ASSERT(len == 3);
        DENG_ASSERT(!qstrncmp(fragment->from, "some", len+1));
        }
    }
    catch(Error const& er)
    {
        qWarning() << er.asText();
    }
}

LIBDENG_RUN_UNITTEST(Uri)

#endif

} // namespace de
