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
#include <de/Reader>
#include <de/Writer>
#include <QDebug>

#include <QList>

#include "de_base.h"
#include "de_filesys.h"
#include "dualstring.h"
#include "game.h"

/// Size of the fixed-size path node buffer.
#define SEGMENT_BUFFER_SIZE         24

namespace de {

Uri::hash_type const Uri::hash_range = 512;

ushort Uri::Segment::hash() const
{
    // Is it time to compute the hash?
    if(!gotHashKey)
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
        gotHashKey = true;
    }
    return hashKey;
}

int Uri::Segment::length() const
{
    if(!qstrcmp(to, "") && !qstrcmp(from, ""))
        return 0;
    return (to - from) + 1;
}

String Uri::Segment::toString() const
{
    return String(from, length());
}

struct Uri::Instance
{
    /// Total number of segments in the path.
    int segmentCount;

    /**
     * Segment map of the path. The map is composed of two parts: the first
     * SEGMENT_BUFFER_SIZE elements are placed into a fixed-size buffer which is
     * allocated along with the Instance, and additional segments are allocated
     * dynamically and linked in the extraSegments list.
     *
     * This optimized representation should mean that the majority of paths
     * can be represented without dynamically allocating memory from the heap.
     */
    Uri::Segment segmentBuffer[SEGMENT_BUFFER_SIZE];

    /**
     * List of the extra segments that don't fit in segmentBuffer, in reverse
     * order.
     */
    QList<Uri::Segment*> extraSegments;

    /// Cached copy of the resolved Uri.
    String resolved;

    /**
     * The cached copy only applies when this game is loaded.
     *
     * @note Add any other conditions here that result in different results for
     * resolveUri().
     */
    void* resolvedForGame;

    DualString scheme;
    DualString path;

    Instance()
        : segmentCount(-1), resolvedForGame(0)
    {
        memset(segmentBuffer, 0, sizeof(segmentBuffer));
    }

    ~Instance()
    {
        clearMap();
    }

    /// @return  @c true iff @a node comes from the fixed-size
    /// Uri::Instance::segmentBuffer.
    inline bool isStaticSegment(Uri::Segment const& node)
    {
        return &node >= segmentBuffer &&
               &node < (segmentBuffer + sizeof(*segmentBuffer) * SEGMENT_BUFFER_SIZE);
    }

    /**
     * Return the segment map to an empty state.
     *
     * @post The map will need to be rebuilt with mapPath().
     */
    void clearMap()
    {
        while(!extraSegments.isEmpty())
        {
            delete extraSegments.takeFirst();
        }
        memset(segmentBuffer, 0, sizeof(segmentBuffer));
        segmentCount = -1;
    }

    /**
     * Allocate a new segment, from either the fixed-size segmentBuffer if it
     * isn't full, or dynamically from the heap.
     *
     * Use isStaticSegment() to determine if the segment is from segmentBuffer.
     *
     * @return  New segment.
     */
    Uri::Segment* allocSegment(char const* from, char const* to)
    {
        Uri::Segment* segment;
        if(segmentCount < SEGMENT_BUFFER_SIZE)
        {
            segment = segmentBuffer + segmentCount;
            memset(segment, 0, sizeof(*segment));
        }
        else
        {
            // Allocate an "extra" node.
            segment = new Uri::Segment();
            extraSegments.append(segment);
        }

        segment->from = from;
        segment->to   = to;

        // There is now one more segment in the map.
        segmentCount++;

        return segment;
    }

    /**
     * Build the path segment map.
     * If the URI is modified (path changed), the existing map is invalidated and
     * needs to be remapped.
     */
    void mapPath()
    {
        // Already been here?
        if(segmentCount >= 0) return;

        segmentCount = 0;
        extraSegments.clear();

        if(path.isEmpty())
        {
            // There always has to be at least one segment.
            allocSegment("", "");
            return;
        }

        char const* segBegin = path.utf8CStr();
        char const* segEnd = segBegin + path.length() - 1;

        // Skip over any trailing delimiters.
        for(int i = path.length(); *segEnd && *segEnd == '/' && i-- > 0; segEnd--)
        {}

        // Scan the path for segments, in reverse order.
        char const* from;
        forever
        {
            // Find the start of the next segment.
            for(from = segEnd; from > segBegin && !(*from == '/'); from--)
            {}

            allocSegment(*from == '/'? from + 1 : from, segEnd);

            // Are there no more parent directories?
            if(from == segBegin) break;

            // So far so good. Move one directory level upwards.
            // The next name ends here.
            segEnd = from - 1;
        }

        // Deal with the special case of a Unix style zero-length root name.
        if(*segBegin == '/')
        {
            allocSegment("", "");
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

        clearCachedResolved();

        int sepPos = path.indexOf(':');
        if(sepPos > URI_MINSCHEMELENGTH)
        {
            scheme = path.left(sepPos);

            if(defaultResourceClass == RC_NULL || App_FileSystem()->schemeByName(scheme))
            {
                path = path.mid(sepPos + 1);
                return;
            }

            LOG_WARNING("Unknown scheme in path \"%s\", using default.") << path;
            path = path.mid(sepPos + 1);
        }
        else
        {
            scheme.clear();
        }

        // Attempt to guess the scheme by interpreting the path?
        if(defaultResourceClass == RC_UNKNOWN)
        {
            defaultResourceClass = DD_GuessFileTypeFromFileName(path).defaultClass();
        }

        if(VALID_RESOURCECLASSID(defaultResourceClass))
        {
            FS1::Scheme* fsScheme = App_FileSystem()->schemeByName(DD_ResourceClassById(defaultResourceClass).defaultScheme());
            DENG_ASSERT(fsScheme);
            scheme = fsScheme->name();
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

private:
    Instance& operator = (const Instance&); // no assignment
    Instance(const Instance&); // no copying
};

Uri::Uri(String path, resourceclassid_t defaultResourceClass, QChar sep)
{
    d = new Instance();
    if(!path.isEmpty())
    {
        setUri(path, defaultResourceClass, sep);
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

Uri Uri::fromNativePath(NativePath const& path, resourceclassid_t defaultResourceClass)
{
    return Uri(path.expand().withSeparators('/'), defaultResourceClass);
}

Uri Uri::fromNativeDirPath(NativePath const& nativeDirPath, resourceclassid_t defaultResourceClass)
{
    // Uri follows the convention of having a slash at the end for directories.
    return Uri(nativeDirPath.expand().withSeparators('/') + '/', defaultResourceClass);
}

Uri::~Uri()
{
    delete d;
}

int Uri::segmentCount() const
{
    d->mapPath();
    return d->segmentCount;
}

Uri::Segment const& Uri::segment(int index) const
{
    d->mapPath();

    if(index < 0 || index >= d->segmentCount)
    {
        /// @throw NotSegmentError  Attempt to reference a nonexistent segment.
        throw NotSegmentError("Uri::segment", String("Index #%1 references a nonexistent segment").arg(index));
    }

    // Is this in the static buffer?
    if(index < SEGMENT_BUFFER_SIZE)
    {
        return d->segmentBuffer[index];
    }

    // No - an extra segment.
    return *d->extraSegments[index - SEGMENT_BUFFER_SIZE];
}

bool Uri::operator == (Uri const& other) const
{
    if(this == &other) return true;

    // First, lets check if the scheme differs.
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

bool Uri::isEmpty() const
{
    return d->path.isEmpty();
}

Uri& Uri::clear()
{
    d->scheme.clear();
    d->path.clear();
    d->clearCachedResolved();
    d->clearMap();
    return *this;
}

String const& Uri::scheme() const
{
    return d->scheme;
}

String const& Uri::path() const
{
    return d->path;
}

char const* Uri::schemeCStr() const
{
    return d->scheme.utf8CStr();
}

char const* Uri::pathCStr() const
{
    return d->path.utf8CStr();
}

ddstring_s const* Uri::schemeStr() const
{
    return d->scheme.toStr();
}

ddstring_s const* Uri::pathStr() const
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

Uri& Uri::setPath(String newPath, QChar sep)
{
    if(sep != '/')
    {
        newPath = newPath.replace(sep, QString("/"), Qt::CaseInsensitive);
    }
    d->path = newPath;
    d->clearCachedResolved();
    d->clearMap();
    return *this;
}

Uri& Uri::setUri(String rawUri, resourceclassid_t defaultResourceClass, QChar sep)
{
    LOG_AS("Uri::setUri");

    if(sep != '/')
    {
        rawUri = rawUri.replace(sep, QString("/"), Qt::CaseInsensitive);
    }

    d->path = rawUri.trimmed();
    d->parseScheme(defaultResourceClass);
    d->clearCachedResolved();
    d->clearMap();
    return *this;
}

String Uri::compose(QChar sep) const
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
    if(sep != '/')
    {
        result = result.replace('/', sep, Qt::CaseInsensitive);
    }
    return result;
}

String Uri::asText() const
{
    // Just compose it for now, we can worry about making it 'pretty' later.
    AutoStr* path = AutoStr_FromTextStd(compose().toUtf8().constData());
    Str_PercentDecode(path);
    return String(Str_Text(path));
}

void Uri::debugPrint(int indent, PrintFlags flags, String unresolvedText) const
{
    indent = MAX_OF(0, indent);

    bool resolvedPath = (flags & OutputResolved) && !d->resolved.isEmpty();
    if(unresolvedText.isEmpty()) unresolvedText = "--(!)incomplete";

    LOG_DEBUG("%s\"%s\"%s%s")
        << String(indent, ' ') << ""
        << ((flags & TransformPathPrettify)? NativePath(asText()).pretty() : asText())
        << ((flags & OutputResolved)? (resolvedPath? "=> " : unresolvedText) : "")
        << ((flags & OutputResolved) && resolvedPath?
                ((flags & TransformPathPrettify)? NativePath(d->resolved).pretty() : NativePath(d->resolved))
              : "");
}

void Uri::operator >> (Writer& to) const
{
    to << d->scheme.toUtf8() << d->path.toUtf8();
}

void Uri::operator << (Reader& from)
{
    clear();

    Block b;
    from >> b;
    setScheme(String::fromUtf8(b));

    from >> b;
    setPath(String::fromUtf8(b));
}

#ifdef _DEBUG

LIBDENG_DEFINE_UNITTEST(Uri)
{
    try
    {
        // Test emptiness.
        {
            Uri u;
            DENG_ASSERT(u.isEmpty());
            DENG_ASSERT(u.segmentCount() == 1);
        }

        // Test a zero-length path.
        {
            Uri u("", RC_NULL);
            DENG_ASSERT(u.isEmpty());
            DENG_ASSERT(u.segmentCount() == 1);
        }

        // Equality and copying.
        {
            Uri a("some/thing", RC_NULL);
            Uri b("/other/thing", RC_NULL);

            DENG_ASSERT(a != b);

            Uri c = a;
            DENG_ASSERT(c == a);
            DENG_ASSERT(c.segment(1).toString() == "some");

            b = a;
            DENG_ASSERT(b == a);
            DENG_ASSERT(b.segment(1).toString() == "some");
        }

        // Swapping.
        {
            Uri a("a/b/c", RC_NULL);
            Uri b("d/e", RC_NULL);

            DENG_ASSERT(a.segmentCount() == 3);
            DENG_ASSERT(a.segment(1).toString() == "b");

            std::swap(a, b);

            DENG_ASSERT(a.segmentCount() == 2);
            DENG_ASSERT(a.segment(1).toString() == "d");
            DENG_ASSERT(b.segmentCount() == 3);
            DENG_ASSERT(b.segment(1).toString() == "b");
        }

        // Test a Windows style path with a drive plus file path.
        {
            Uri u("c:/something.ext", RC_NULL);
            DENG_ASSERT(u.segmentCount() == 2);

            DENG_ASSERT(u.segment(0).length() == 13);
            DENG_ASSERT(u.segment(0).toString() == "something.ext");

            DENG_ASSERT(u.segment(1).length() == 2);
            DENG_ASSERT(u.segment(1).toString() == "c:");
        }

        // Test a Unix style path with a zero-length root node name.
        {
            Uri u("/something.ext", RC_NULL);
            DENG_ASSERT(u.segmentCount() == 2);

            DENG_ASSERT(u.segment(0).length() == 13);
            DENG_ASSERT(u.segment(0).toString() == "something.ext");

            DENG_ASSERT(u.segment(1).length() == 0);
            DENG_ASSERT(u.segment(1).toString() == "");
        }

        // Test a relative directory.
        {
            Uri u("some/dir/structure/", RC_NULL);
            DENG_ASSERT(u.segmentCount() == 3);

            DENG_ASSERT(u.segment(0).length() == 9);
            DENG_ASSERT(u.segment(0).toString() == "structure");

            DENG_ASSERT(u.segment(1).length() == 3);
            DENG_ASSERT(u.segment(1).toString() == "dir");

            DENG_ASSERT(u.segment(2).length() == 4);
            DENG_ASSERT(u.segment(2).toString() == "some");
        }

        // Test the extra segments.
        {
            Uri u("/30/29/28/27/26/25/24/23/22/21/20/19/18/17/16/15/14/13/12/11/10/9/8/7/6/5/4/3/2/1/0", RC_NULL);
            DENG_ASSERT(u.segmentCount() == 32);

            DENG_ASSERT(u.segment(0).toString()  == "0");
            DENG_ASSERT(u.segment(23).toString() == "23");
            DENG_ASSERT(u.segment(24).toString() == "24");
            DENG_ASSERT(u.segment(30).toString() == "30");
        }
    }
    catch(Error const& er)
    {
        qWarning() << er.asText();
        return false;
    }
    return true;
}

LIBDENG_RUN_UNITTEST(Uri)

#endif // _DEBUG

} // namespace de
