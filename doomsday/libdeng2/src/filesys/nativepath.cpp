/**
 * @file nativepath.cpp
 * File paths for the native file system.
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

#include "de/NativePath"
#include "de/App"
#include <QDir>

#ifdef WIN32
#  define DIR_SEPARATOR QChar('\\')
#else
#  define DIR_SEPARATOR QChar('/')
#  ifdef UNIX
#    include <sys/types.h>
#    include <pwd.h>
#  endif
#endif

namespace de {

static QString toNative(QString const &s)
{
    // This will resolve parent references (".."), multiple separators
    // (hello//world), and self-references (".").
    return QDir::toNativeSeparators(QDir::cleanPath(s));
}

NativePath::NativePath() : Path()
{}

NativePath::NativePath(QString const &str) : Path(toNative(str), DIR_SEPARATOR)
{}

NativePath::NativePath(char const *nullTerminatedCStr)
    : Path(toNative(nullTerminatedCStr), DIR_SEPARATOR)
{
}

NativePath::NativePath(char const *cStr, dsize length)
    : Path(toNative(QString::fromUtf8(cStr, length)), DIR_SEPARATOR)
{}

NativePath &NativePath::operator = (QString const &str)
{
    set(toNative(str), DIR_SEPARATOR);
    return *this;
}

NativePath &NativePath::operator = (char const *nullTerminatedCStr)
{
    return (*this) = QString(nullTerminatedCStr);
}

NativePath NativePath::concatenatePath(NativePath const &nativePath) const
{
    if(nativePath.isAbsolute()) return nativePath;
    return toString().concatenatePath(nativePath, QChar(DIR_SEPARATOR));
}

NativePath NativePath::concatenatePath(QString const &nativePath) const
{
    return concatenatePath(NativePath(nativePath));
}

NativePath NativePath::operator / (NativePath const &nativePath) const
{
    return concatenatePath(nativePath);
}

NativePath NativePath::operator / (QString const &str) const
{
    return *this / NativePath(str);
}

NativePath NativePath::operator / (char const *nullTerminatedCStr) const
{
    return *this / NativePath(nullTerminatedCStr);
}

NativePath NativePath::fileNamePath() const
{
    return String(*this).fileNamePath(DIR_SEPARATOR);
}

bool NativePath::isAbsolute() const
{
    return QDir::isAbsolutePath(expand());
}

NativePath NativePath::expand(bool *didExpand) const
{
    if(first() == '>' || first() == '}')
    {
        if(didExpand) *didExpand = true;
        return App::app().nativeBasePath() / toString().mid(1);
    }
#ifdef UNIX
    else if(first() == '~')
    {
        if(didExpand) *didExpand = true;

        const String path = toString();
        int firstSlash = path.indexOf('/');
        if(firstSlash > 1)
        {
            // Parse the user's home directory (from passwd).
            QByteArray userName = path.mid(1, firstSlash - 1).toLatin1();
            struct passwd *pw = getpwnam(userName);
            if(!pw)
            {
                /// @throws UnknownUserError  User is not known.
                throw UnknownUserError("NativePath::expand",
                                       String("Unknown user '%1'").arg(QLatin1String(userName)));
            }

            return NativePath(pw->pw_dir) / path.mid(firstSlash + 1);
        }
        else
        {
            // Replace with the HOME path.
            return NativePath(QDir::homePath()) / path.mid(2);
        }
    }
#endif

    // No expansion done.
    if(didExpand) *didExpand = false;
    return *this;
}

String NativePath::pretty() const
{
    if(isEmpty()) return *this;

    String result = *this;

    // Hide relative directives like '}'
    if(result.length() > 1 && (result.first() == '}' || result.first() == '>'))
    {
        result = result.mid(1);
    }

    // If within our the base directory cut out the base path.
    if(QDir::isAbsolutePath(result))
    {
        NativePath basePath = App::app().nativeBasePath();
        if(result.beginsWith(basePath))
        {
            result = result.mid(basePath.length() + 1);
        }
    }

    return result;
}

String NativePath::withSeparators(QChar sep) const
{
    return Path::withSeparators(sep);
}

NativePath NativePath::workPath()
{
    return QDir::currentPath();
}

}
