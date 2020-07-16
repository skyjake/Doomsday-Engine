/** @file uri.cpp Universal Resource Identifier.
 * @ingroup base
 *
 * @authors Copyright &copy; 2010-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2010-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/uri.h"
#include "doomsday/filesys/fs_main.h"
#include "doomsday/dualstring.h"
#include "doomsday/game.h"
#include "doomsday/doomsdayapp.h"

#include <de/legacy/str.h>
#include <de/legacy/reader.h>
#include <de/legacy/writer.h>
#include <de/legacy/unittest.h>
#include <de/nativepath.h>
#include <de/reader.h>
#include <de/regexp.h>
#include <de/writer.h>
#include <de/app.h>

namespace res {

using namespace de;

static Uri::ResolverFunc resolverFunc;

/**
 * Extracts the scheme from a string.
 *
 * @param stringWithScheme  The scheme is removed from the string.
 *
 * @return Scheme, or empty string if no valid scheme was present.
 */
static String extractScheme(String &stringWithScheme)
{
    String scheme;
    auto pos = stringWithScheme.indexOf(':');
    if (pos && pos > URI_MINSCHEMELENGTH) // could be Windows-style driver letter "c:"
    {
        scheme = stringWithScheme.left(pos);
        stringWithScheme.remove(BytePos(0), pos + 1);
    }
    return scheme;
}

DE_PIMPL_NOREF(Uri)
{
    DE_NO_ASSIGN(Impl)

    Path path; ///< Path of the Uri.

    String strPath; // Redundant; for legacy access, remove this!
    String scheme; ///< Scheme of the Uri.

    /// Cached copy of the resolved path.
    Path resolvedPath;

    /**
     * The cached path only applies when this game is loaded.
     *
     * @note Add any other conditions here that result in different results for
     * resolveUri().
     */
    const Game *resolvedForGame;

    Impl() : resolvedForGame(nullptr)
    {}

    Impl(const Impl &other)
        : de::IPrivate(),
          path           (other.path),
          strPath        (other.strPath),
          scheme         (other.scheme),
          resolvedPath   (other.resolvedPath),
          resolvedForGame(other.resolvedForGame)
    {}

    void clearCachedResolved()
    {
        resolvedPath.clear();
        resolvedForGame = nullptr;
    }

    void parseRawUri(String rawUri, Char sep, resourceclassid_t defaultResourceClass)
    {
        LOG_AS("Uri::parseRawUri");

        clearCachedResolved();

        scheme = extractScheme(rawUri); // scheme removed
        if (sep != '/') rawUri.replace(sep, '/'); // force slashes as separator
        path = rawUri;
        strPath = path.toString(); // for legacy code

        if (!scheme.isEmpty())
        {
            if (defaultResourceClass == RC_NULL || App_FileSystem().knownScheme(scheme))
            {
                // Scheme is accepted as is.
                return;
            }
            LOG_RES_WARNING("Unknown scheme \"%s\" for path \"%s\", using default scheme instead") << scheme << strPath;
        }

        // Attempt to guess the scheme by interpreting the path?
        if (defaultResourceClass == RC_IMPLICIT)
        {
            defaultResourceClass = DD_GuessFileTypeFromFileName(strPath).defaultClass();
        }

        if (VALID_RESOURCECLASSID(defaultResourceClass))
        {
            FS1::Scheme &fsScheme = App_FileSystem().scheme(ResourceClass::classForId(defaultResourceClass).defaultScheme());
            scheme = fsScheme.name();
        }
    }

    String resolveSymbol(const String &symbol) const
    {
        if (!resolverFunc)
        {
            return symbol;
        }
        return resolverFunc(symbol);
    }

    inline String parseExpression(const String &expression) const
    {
        // Presently the expression consists of a single symbol.
        return resolveSymbol(expression);
    }

    String resolve() const
    {
        LOG_AS("Uri::resolve");

        String result;

        // Keep scanning the path for embedded expressions.
        String expression;
        //BytePos expEnd{0}, expBegin;
        //mb_iterator expEnd, i;
        /*while ((expBegin = strPath.indexOf("$", expEnd)) != BytePos::npos)
        {
            // Is the next char the start-of-expression character?
            if (*(i + 1) == '(')
            {
                // Copy everything up to the '$'.
                result += strPath.substr(expEnd, expBegin - expEnd);

                // Skip over the '$'.
                ++expBegin;

                // Find the end-of-expression character.
                expEnd = strPath.indexOf(")", expBegin);
                if (!expEnd)
                {
                    LOG_RES_WARNING("Ignoring expression \"" + strPath + "\": "
                                    "missing a closing ')'");
                    expEnd = strPath.sizeb();
                }

                // Skip over the '('.
                ++expBegin;

                // The range of the expression substring is now known.
                expression = strPath.substr(expBegin, expEnd - expBegin);

                result += parseExpression(expression);
            }
            else
            {
                // No - copy the '$' and continue.
                result += '$';
            }

            ++expEnd;
        }*/

        static const RegExp reExpr(R"(\$\(([A-Za-z.]+)\))");

        const char *pos = strPath.begin();
        RegExpMatch match;
        while (reExpr.match(strPath, match))
        {
            result += CString(pos, match.begin());
            result += parseExpression(match.captured(1));
            pos = match.end();
        }
        result += CString(pos, strPath.end());

        // Copy anything remaining.
        //result += strPath.substr(expEnd);

        return result;
    }
};

Uri::Uri() : d(new Impl)
{}

Uri::Uri(const String &percentEncoded) : d(new Impl)
{
    if (!percentEncoded.isEmpty())
    {
        setUri(percentEncoded, RC_IMPLICIT, '/');
    }
}

Uri::Uri(const String &percentEncoded, resourceclassid_t defaultResourceClass, Char sep)
    : d(new Impl)
{
    if (!percentEncoded.isEmpty())
    {
        setUri(percentEncoded, defaultResourceClass, sep);
    }
}

Uri::Uri(const String &scheme, const Path &path) : d(new Impl)
{
    setScheme(scheme);
    setPath(path);
}

Uri::Uri(resourceclassid_t resClass, const Path &path) : d(new Impl)
{
    setUri(path.toString(), resClass, path.separator());
}

Uri::Uri(const Path &path) : d(new Impl)
{
    setPath(path);
}

Uri::Uri(const char *nullTerminatedCStr) : d(new Impl)
{
    setUri(nullTerminatedCStr);
}

Uri Uri::fromUserInput(char **argv, int argc, bool (*knownScheme)(const String &name))
{
    return fromUserInput(makeList(argc, argv), knownScheme);
}

Uri Uri::fromUserInput(const StringList &args, bool (*knownScheme)(const String &name))
{
    Uri output;
    if (args)
    {
        // [0: <scheme>:<path>] or [0: <scheme>] or [0: <path>].
        switch (args.sizei())
        {
        case 1: {
            // Try to extract the scheme and encode the rest of the path.
            String rawUri(args[0]);
            if (auto pos = rawUri.indexOf(':'))
            {
                output.setScheme(rawUri.left(pos));
                rawUri.remove(BytePos(0), pos + 1);
                output.setPath(Path::normalize(rawUri.toPercentEncoding()));
            }
            // Just a scheme name?
            else if (knownScheme && knownScheme(rawUri))
            {
                output.setScheme(rawUri);
            }
            else
            {
                // Just a path.
                output.setPath(Path::normalize(rawUri.toPercentEncoding()));
            }
            break; }

        // [0: <scheme>, 1: <path>]
        case 2:
            // Assign the scheme and encode the path.
            output.setScheme(args[0]);
            output.setPath(Path::normalize(args[1].toPercentEncoding()));
            break;

        default: break;
        }
    }
    return output;
}

Uri::Uri(const Uri &other) : d(new Impl(*other.d))
{}

Uri Uri::fromNativePath(const NativePath &path, resourceclassid_t defaultResourceClass)
{
    return Uri(path.expand().withSeparators('/'), defaultResourceClass);
}

Uri Uri::fromNativeDirPath(const NativePath &nativeDirPath, resourceclassid_t defaultResourceClass)
{
    // Uri follows the convention of having a slash at the end for directories.
    return Uri(nativeDirPath.expand().withSeparators('/') + "/", defaultResourceClass);
}

bool Uri::isEmpty() const
{
    return d->path.isEmpty();
}

bool Uri::operator == (const Uri &other) const
{
    if (this == &other) return true;

    // First, lets check if the scheme differs.
    if (d->scheme.compareWithoutCase(other.d->scheme)) return false;

    // We can skip resolving if the paths are identical.
    if (d->path == other.d->path) return true;

    // We must be able to resolve both paths to compare.
    try
    {
        // Do not match partial paths.
        if (resolved().length() != other.resolved().length()) return false;

        return resolved().compareWithoutCase(other.resolved()) == 0;
    }
    catch (const ResolveError &)
    {
        // Ignore the error.
    }
    return false;
}

Uri &Uri::clear()
{
    d->path.clear();
    d->strPath.clear();
    d->scheme.clear();
    d->clearCachedResolved();
    return *this;
}

const String &Uri::scheme() const
{
    return d->scheme;
}

const Path &Uri::path() const
{
    return d->path;
}

const char *Uri::schemeCStr() const
{
    return d->scheme;
}

const char *Uri::pathCStr() const
{
    return d->strPath;
}

/*const ddstring_s *Uri::schemeStr() const
{
    return d->scheme.toStr();
}

const ddstring_s *Uri::pathStr() const
{
    return d->strPath.toStr();
}*/

String Uri::resolved() const
{
    const Game *currentGame =
        (!App::appExists() || DoomsdayApp::game().isNull() ? nullptr : &DoomsdayApp::game());

#ifndef DE_DISABLE_URI_RESOLVE_CACHING
    if (d->resolvedForGame && d->resolvedForGame == currentGame)
    {
        // We can just return the previously prepared resolved URI.
        return d->resolvedPath;
    }
#endif

    d->clearCachedResolved();

    // Keep a copy of this, we'll likely need it many, many times.
    d->resolvedPath = d->resolve();

    DE_ASSERT(d->resolvedPath.separator() == Char('/'));

    d->resolvedForGame = currentGame;

    return d->resolvedPath.toString();
}

Uri &Uri::setScheme(String newScheme)
{
    d->scheme = std::move(newScheme);
    d->clearCachedResolved();
    return *this;
}

Uri &Uri::setPath(const Path &newPath)
{
    // Force to slashes.
    d->path = newPath.withSeparators('/');

    d->strPath = d->path.toString(); // legacy support
    d->clearCachedResolved();
    return *this;
}

Uri &Uri::setPath(const String& newPath, Char sep)
{
    return setPath(Path(newPath.strip(), sep));
}

Uri &Uri::setPath(const CString &newPath, Char sep)
{
    return setPath(newPath.toString(), sep);
}

Uri &Uri::setPath(const char *newPathUtf8, char sep)
{
    return setPath(Path(String(newPathUtf8).strip(), sep));
}

Uri &Uri::setUri(const String& rawUri, resourceclassid_t defaultResourceClass, Char sep)
{
    LOG_AS("Uri::setUri");
    d->parseRawUri(rawUri.strip(), sep, defaultResourceClass);
    return *this;
}

String Uri::compose(ComposeAsTextFlags compositionFlags, Char sep) const
{
    String text;
    if (!(compositionFlags & OmitScheme))
    {
        if (!d->scheme.isEmpty())
        {
            text += d->scheme + ":";
        }
    }
    if (!(compositionFlags & OmitPath))
    {
        String path = d->path.withSeparators(sep);
        if (compositionFlags & DecodePath)
        {
            path = String::fromPercentEncoding(path);
        }
        text += path;
    }
    return text;
}

String Uri::asText() const
{
    return compose(DefaultComposeAsTextFlags | DecodePath);
}

void Uri::operator >> (Writer &to) const
{
    to << d->scheme << d->path;
}

void Uri::operator << (Reader &from)
{
    clear();

    from >> d->scheme >> d->path;

    d->strPath = d->path;
}

void Uri::setResolverFunc(ResolverFunc resolver)
{
    resolverFunc = resolver;
}

void Uri::readUri(reader_s *reader, const String& defaultScheme)
{
    clear();

    ddstring_t scheme;
    Str_InitStd(&scheme);
    Str_Read(&scheme, reader);

    ddstring_t path;
    Str_InitStd(&path);
    Str_Read(&path, reader);

    if (Str_IsEmpty(&scheme) && !defaultScheme.isEmpty())
    {
        Str_Set(&scheme, defaultScheme);
    }

    setScheme(Str_Text(&scheme));
    setPath  (Str_Text(&path  ));
}

void Uri::writeUri(writer_s *writer, int omitComponents) const
{
    if (omitComponents & UCF_SCHEME)
    {
        ddstring_t emptyString;
        Str_InitStatic(&emptyString, "");
        Str_Write(&emptyString, writer);
    }
    else
    {
        Str_Write(AutoStr_FromTextStd(scheme()), writer);
    }
    Str_Write(AutoStr_FromTextStd(path()), writer);
}

#ifdef _DEBUG
#  if !defined (DE_MOBILE)

DE_DEFINE_UNITTEST(Uri)
{
    try
    {
        // Test emptiness.
        {
            Uri u;
            DE_ASSERT(u.isEmpty());
            DE_ASSERT(u.path().segmentCount() == 1);
        }

        // Test a zero-length path.
        {
            Uri u("", RC_NULL);
            DE_ASSERT(u.isEmpty());
            DE_ASSERT(u.path().segmentCount() == 1);
        }

        // Equality and copying.
        {
            Uri a("some/thing", RC_NULL);
            Uri b("/other/thing", RC_NULL);

            DE_ASSERT(a != b);

            Uri c = a;
            DE_ASSERT(c == a);
            DE_ASSERT(c.path().reverseSegment(1).toLowercaseString() == "some");

            b = a;
            DE_ASSERT(b == a);
            //qDebug() << b.reverseSegment(1);
            DE_ASSERT(b.path().reverseSegment(1).toLowercaseString() == "some");
        }

        // Swapping.
        {
            Uri a("a/b/c", RC_NULL);
            Uri b("d/e", RC_NULL);

            DE_ASSERT(a.path().segmentCount() == 3);
            DE_ASSERT(a.path().reverseSegment(1).toLowercaseString() == "b");

            std::swap(a, b);

            DE_ASSERT(a.path().segmentCount() == 2);
            DE_ASSERT(a.path().reverseSegment(1).toLowercaseString() == "d");
            DE_ASSERT(b.path().segmentCount() == 3);
            DE_ASSERT(b.path().reverseSegment(1).toLowercaseString() == "b");
        }

        // Test a Windows style path with a drive plus file path.
        {
            Uri u("c:/something.ext", RC_NULL);
            DE_ASSERT(u.path().segmentCount() == 2);

            DE_ASSERT(u.path().reverseSegment(0).length() == 13);
            DE_ASSERT(u.path().reverseSegment(0).toLowercaseString() == "something.ext");

            DE_ASSERT(u.path().reverseSegment(1).length() == 2);
            DE_ASSERT(u.path().reverseSegment(1).toLowercaseString() == "c:");
        }

        // Test a Unix style path with a zero-length root node name.
        {
            Uri u("/something.ext", RC_NULL);
            DE_ASSERT(u.path().segmentCount() == 2);

            DE_ASSERT(u.path().reverseSegment(0).length() == 13);
            DE_ASSERT(u.path().reverseSegment(0).toLowercaseString() == "something.ext");

            DE_ASSERT(u.path().reverseSegment(1).length() == 0);
            DE_ASSERT(u.path().reverseSegment(1).toLowercaseString() == "");
        }

        // Test a relative directory.
        {
            Uri u("some/dir/structure/", RC_NULL);
            DE_ASSERT(u.path().segmentCount() == 3);

            DE_ASSERT(u.path().reverseSegment(0).length() == 9);
            DE_ASSERT(u.path().reverseSegment(0).toLowercaseString() == "structure");

            DE_ASSERT(u.path().reverseSegment(1).length() == 3);
            DE_ASSERT(u.path().reverseSegment(1).toLowercaseString() == "dir");

            DE_ASSERT(u.path().reverseSegment(2).length() == 4);
            DE_ASSERT(u.path().reverseSegment(2).toLowercaseString() == "some");
        }
    }
    catch (const Error &er)
    {
        er.warnPlainText();
        return false;
    }
    return true;
}

DE_RUN_UNITTEST(Uri)

#  endif // DE_MOBILE
#endif // _DEBUG

} // namespace res
