/** @file uri.cpp Universal Resource Identifier.
 * @ingroup base
 *
 * @authors Copyright &copy; 2010-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2010-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "Game"

namespace de {

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
    int pos = stringWithScheme.indexOf(':');
    if(pos > URI_MINSCHEMELENGTH) // could be Windows-style driver letter "c:"
    {
        scheme = stringWithScheme.left(pos);
        stringWithScheme.remove(0, pos + 1);
    }
    return scheme;
}

DENG2_PIMPL_NOREF(Uri)
{
    Path path; ///< Path of the Uri.

    DualString strPath; // Redundant; for legacy access, remove this!

    DualString scheme; ///< Scheme of the Uri.

    /// Cached copy of the resolved path.
    Path resolvedPath;

    /**
     * The cached path only applies when this game is loaded.
     *
     * @note Add any other conditions here that result in different results for
     * resolveUri().
     */
    void *resolvedForGame;

    Instance() : resolvedForGame(0)
    {}

    Instance(const Instance &other)
        : path           (other.path),
          strPath        (other.strPath),
          scheme         (other.scheme),
          resolvedPath   (other.resolvedPath),
          resolvedForGame(other.resolvedForGame)
    {}

    void clearCachedResolved()
    {
        resolvedPath.clear();
        resolvedForGame = 0;
    }

    void parseRawUri(String rawUri, QChar sep, resourceclassid_t defaultResourceClass)
    {
        LOG_AS("Uri::parseRawUri");

        clearCachedResolved();

        scheme = extractScheme(rawUri); // scheme removed
        if(sep != '/') rawUri.replace(sep, '/'); // force slashes as separator
        path = rawUri;
        strPath = path.toString(); // for legacy code

        if(!scheme.isEmpty())
        {
            if(defaultResourceClass == RC_NULL || App_FileSystem().knownScheme(scheme))
            {
                // Scheme is accepted as is.
                return;
            }
            LOG_WARNING("Unknown scheme \"%s\" for path \"%s\", using default instead.") << scheme << strPath;
        }

        // Attempt to guess the scheme by interpreting the path?
        if(defaultResourceClass == RC_UNKNOWN)
        {
            defaultResourceClass = DD_GuessFileTypeFromFileName(strPath).defaultClass();
        }

        if(VALID_RESOURCECLASSID(defaultResourceClass))
        {
            FS1::Scheme &fsScheme = App_FileSystem().scheme(DD_ResourceClassById(defaultResourceClass).defaultScheme());
            scheme = fsScheme.name();
        }
    }

    String resolveSymbol(QStringRef const &symbol) const
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
            if(!App_GameLoaded())
            {
                /// @throw ResolveSymbolError  An unresolveable symbol was encountered.
                throw ResolveSymbolError("Uri::resolveSymbol", "Symbol 'Game' did not resolve (no game loaded)");
            }

            return Str_Text(App_CurrentGame().identityKey());
        }
        else if(!symbol.compare("GamePlugin.Name", Qt::CaseInsensitive))
        {
            if(!App_GameLoaded() || !gx.GetVariable)
            {
                /// @throw ResolveSymbolError  An unresolveable symbol was encountered.
                throw ResolveSymbolError("Uri::resolveSymbol", "Symbol 'GamePlugin' did not resolve (no game plugin loaded)");
            }

            return String((char *)gx.GetVariable(DD_PLUGIN_NAME));
        }
        else
        {
            /// @throw UnknownSymbolError  An unknown symbol was encountered.
            throw UnknownSymbolError("Uri::resolveSymbol", "Symbol '" + symbol.toString() + "' is unknown");
        }
    }

    inline String parseExpression(QStringRef const &expression) const
    {
        // Presently the expression consists of a single symbol.
        return resolveSymbol(expression);
    }

    String resolve() const
    {
        LOG_AS("Uri::resolve");

        String result;

        // Keep scanning the path for embedded expressions.
        QStringRef expression;
        int expEnd = 0, expBegin;
        while((expBegin = strPath.indexOf('$', expEnd)) >= 0)
        {
            // Is the next char the start-of-expression character?
            if(strPath.at(expBegin + 1) == '(')
            {
                // Copy everything up to the '$'.
                result += strPath.mid(expEnd, expBegin - expEnd);

                // Skip over the '$'.
                ++expBegin;

                // Find the end-of-expression character.
                expEnd = strPath.indexOf(')', expBegin);
                if(expEnd < 0)
                {
                    LOG_WARNING("Missing closing ')' in expression \"" + strPath + "\", ignoring.");
                    expEnd = strPath.length();
                }

                // Skip over the '('.
                ++expBegin;

                // The range of the expression substring is now known.
                expression = strPath.midRef(expBegin, expEnd - expBegin);

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
        result += strPath.mid(expEnd);

        return result;
    }

private:
    Instance &operator = (Instance const &); // no assignment
};

Uri::Uri() : d(new Instance)
{}

Uri::Uri(String const &percentEncoded, resourceclassid_t defaultResourceClass, QChar sep)
    : d(new Instance)
{
    if(!percentEncoded.isEmpty())
    {
        setUri(percentEncoded, defaultResourceClass, sep);
    }
}

Uri::Uri(String const &scheme, Path const &path) : d(new Instance)
{
    setScheme(scheme);
    setPath(path);
}

Uri::Uri(resourceclassid_t resClass, Path const &path) : d(new Instance)
{
    setUri(path.toString(), resClass, path.separator());
}

Uri::Uri(Path const &path) : d(new Instance)
{
    setPath(path);
}

Uri::Uri(char const *nullTerminatedCStr) : d(new Instance)
{
    setUri(nullTerminatedCStr);
}

Uri Uri::fromUserInput(char **argv, int argc, bool (*knownScheme) (String name))
{
    Uri output;
    if(argv)
    {
        // [0: <scheme>:<path>] or [0: <scheme>] or [0: <path>].
        switch(argc)
        {
        case 1: {
            // Try to extract the scheme and encode the rest of the path.
            String rawUri(argv[0]);
            int pos = rawUri.indexOf(':');
            if(pos >= 0)
            {
                output.setScheme(rawUri.left(pos));
                rawUri.remove(0, pos + 1);
                output.setPath(Path(QString(QByteArray(rawUri.toUtf8()).toPercentEncoding())));
            }
            // Just a scheme name?
            else if(knownScheme && knownScheme(rawUri))
            {
                output.setScheme(rawUri);
            }
            else
            {
                // Just a path.
                output.setPath(Path(QString(QByteArray(rawUri.toUtf8()).toPercentEncoding())));
            }
            break; }

        // [0: <scheme>, 1: <path>]
        case 2:
            // Assign the scheme and encode the path.
            output.setScheme(argv[0]);
            output.setPath(Path(QString(QByteArray(argv[1]).toPercentEncoding())));
            break;

        default: break;
        }
    }
    return output;
}

Uri::Uri(Uri const &other) : LogEntry::Arg::Base(), d(new Instance(*other.d))
{}

Uri Uri::fromNativePath(NativePath const &path, resourceclassid_t defaultResourceClass)
{
    return Uri(path.expand().withSeparators('/'), defaultResourceClass);
}

Uri Uri::fromNativeDirPath(NativePath const &nativeDirPath, resourceclassid_t defaultResourceClass)
{
    // Uri follows the convention of having a slash at the end for directories.
    return Uri(nativeDirPath.expand().withSeparators('/') + '/', defaultResourceClass);
}

bool Uri::isEmpty() const
{
    return d->path.isEmpty();
}

bool Uri::operator == (Uri const &other) const
{
    if(this == &other) return true;

    // First, lets check if the scheme differs.
    if(d->scheme.compareWithoutCase(other.d->scheme)) return false;

    // We can skip resolving if the paths are identical.
    if(d->path == other.d->path) return true;

    // We must be able to resolve both paths to compare.
    try
    {
        // Do not match partial paths.
        if(resolvedRef().length() != other.resolvedRef().length()) return false;

        return resolvedRef().compareWithoutCase(other.resolvedRef()) == 0;
    }
    catch(ResolveError const &)
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

String const &Uri::scheme() const
{
    return d->scheme;
}

Path const &Uri::path() const
{
    return d->path;
}

char const *Uri::schemeCStr() const
{
    return d->scheme.utf8CStr();
}

char const *Uri::pathCStr() const
{
    return d->strPath.utf8CStr();
}

ddstring_s const *Uri::schemeStr() const
{
    return d->scheme.toStr();
}

ddstring_s const *Uri::pathStr() const
{
    return d->strPath.toStr();
}

String Uri::resolved() const
{
    return resolvedRef();
}

String const &Uri::resolvedRef() const
{
#ifndef LIBDENG_DISABLE_URI_RESOLVE_CACHING
    if(d->resolvedForGame && d->resolvedForGame == (void *) (App_GameLoaded()? &App_CurrentGame() : 0))
    {
        // We can just return the previously prepared resolved URI.
        return d->resolvedPath.toStringRef();
    }
#endif

    d->clearCachedResolved();

    // Keep a copy of this, we'll likely need it many, many times.
    d->resolvedPath = d->resolve();

    DENG2_ASSERT(d->resolvedPath.separator() == QChar('/'));

    d->resolvedForGame = (void *) (App_GameLoaded()? &App_CurrentGame() : 0);

    return d->resolvedPath.toStringRef();
}

Uri &Uri::setScheme(String newScheme)
{
    d->scheme = newScheme;
    d->clearCachedResolved();
    return *this;
}

Uri &Uri::setPath(Path const &newPath)
{
    // Force to slashes.
    d->path = newPath.withSeparators('/');

    d->strPath = d->path.toStringRef(); // legacy support
    d->clearCachedResolved();
    return *this;
}

Uri &Uri::setPath(String newPath, QChar sep)
{
    return setPath(Path(newPath.trimmed(), sep));
}

Uri &Uri::setPath(char const *newPathUtf8, char sep)
{
    return setPath(Path(QString::fromUtf8(newPathUtf8).trimmed(), sep));
}

Uri &Uri::setUri(String rawUri, resourceclassid_t defaultResourceClass, QChar sep)
{
    LOG_AS("Uri::setUri");
    d->parseRawUri(rawUri.trimmed(), sep, defaultResourceClass);
    return *this;
}

String Uri::compose(ComposeAsTextFlags compositionFlags, QChar sep) const
{
    String text;
    if(!(compositionFlags & OmitScheme))
    {
        if(!d->scheme.isEmpty())
        {
            text += d->scheme + ":";
        }
    }
    if(!(compositionFlags & OmitPath))
    {
        QString path = d->path;
        if(compositionFlags & DecodePath)
        {
            path = QByteArray::fromPercentEncoding(path.toUtf8());
        }
        if(sep != '/') path.replace('/', sep);
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

#ifdef _DEBUG

LIBDENG_DEFINE_UNITTEST(Uri)
{
    try
    {
        // Test emptiness.
        {
            Uri u;
            DENG_ASSERT(u.isEmpty());
            DENG_ASSERT(u.path().segmentCount() == 1);
        }

        // Test a zero-length path.
        {
            Uri u("", RC_NULL);
            DENG_ASSERT(u.isEmpty());
            DENG_ASSERT(u.path().segmentCount() == 1);
        }

        // Equality and copying.
        {
            Uri a("some/thing", RC_NULL);
            Uri b("/other/thing", RC_NULL);

            DENG_ASSERT(a != b);

            Uri c = a;
            DENG_ASSERT(c == a);
            DENG_ASSERT(c.path().reverseSegment(1).toString() == "some");

            b = a;
            DENG_ASSERT(b == a);
            //qDebug() << b.reverseSegment(1);
            DENG_ASSERT(b.path().reverseSegment(1).toString() == "some");
        }

        // Swapping.
        {
            Uri a("a/b/c", RC_NULL);
            Uri b("d/e", RC_NULL);

            DENG_ASSERT(a.path().segmentCount() == 3);
            DENG_ASSERT(a.path().reverseSegment(1).toString() == "b");

            std::swap(a, b);

            DENG_ASSERT(a.path().segmentCount() == 2);
            DENG_ASSERT(a.path().reverseSegment(1).toString() == "d");
            DENG_ASSERT(b.path().segmentCount() == 3);
            DENG_ASSERT(b.path().reverseSegment(1).toString() == "b");
        }

        // Test a Windows style path with a drive plus file path.
        {
            Uri u("c:/something.ext", RC_NULL);
            DENG_ASSERT(u.path().segmentCount() == 2);

            DENG_ASSERT(u.path().reverseSegment(0).length() == 13);
            DENG_ASSERT(u.path().reverseSegment(0).toString() == "something.ext");

            DENG_ASSERT(u.path().reverseSegment(1).length() == 2);
            DENG_ASSERT(u.path().reverseSegment(1).toString() == "c:");
        }

        // Test a Unix style path with a zero-length root node name.
        {
            Uri u("/something.ext", RC_NULL);
            DENG_ASSERT(u.path().segmentCount() == 2);

            DENG_ASSERT(u.path().reverseSegment(0).length() == 13);
            DENG_ASSERT(u.path().reverseSegment(0).toString() == "something.ext");

            DENG_ASSERT(u.path().reverseSegment(1).length() == 0);
            DENG_ASSERT(u.path().reverseSegment(1).toString() == "");
        }

        // Test a relative directory.
        {
            Uri u("some/dir/structure/", RC_NULL);
            DENG_ASSERT(u.path().segmentCount() == 3);

            DENG_ASSERT(u.path().reverseSegment(0).length() == 9);
            DENG_ASSERT(u.path().reverseSegment(0).toString() == "structure");

            DENG_ASSERT(u.path().reverseSegment(1).length() == 3);
            DENG_ASSERT(u.path().reverseSegment(1).toString() == "dir");

            DENG_ASSERT(u.path().reverseSegment(2).length() == 4);
            DENG_ASSERT(u.path().reverseSegment(2).toString() == "some");
        }
    }
    catch(Error const &er)
    {
        qWarning() << er.asText();
        return false;
    }
    return true;
}

LIBDENG_RUN_UNITTEST(Uri)

#endif // _DEBUG

} // namespace de
