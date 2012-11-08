/**
 * @file nativepath.h
 * File paths for the native file system. @ingroup fs
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
#  define DIR_SEPARATOR '\\'
#else
#  define DIR_SEPARATOR '/'
#  ifdef UNIX
#    include <sys/types.h>
#    include <pwd.h>
#  endif
#endif

namespace de {

NativePath::NativePath()
{}

NativePath::NativePath(const QString &str) : String(QDir::toNativeSeparators(str))
{}

NativePath::NativePath(const char *nullTerminatedCStr)
    : String(QDir::toNativeSeparators(nullTerminatedCStr))
{
}

NativePath& NativePath::operator = (const QString& str)
{
    *static_cast<String*>(this) = QDir::toNativeSeparators(str);
    return *this;
}

NativePath& NativePath::operator = (const char *nullTerminatedCStr)
{
    return (*this) = QString(nullTerminatedCStr);
}

NativePath NativePath::concatenatePath(const NativePath& nativePath) const
{
    return String::concatenatePath(nativePath, DIR_SEPARATOR);
}

NativePath NativePath::concatenatePath(const QString& nativePath) const
{
    return concatenatePath(NativePath(nativePath));
}

NativePath NativePath::operator / (const NativePath& nativePath) const
{
    return String::operator / (nativePath);
}

NativePath NativePath::operator / (const QString& str) const
{
    return *this / NativePath(str);
}

NativePath NativePath::operator / (const char *nullTerminatedCStr) const
{
    return *this / NativePath(nullTerminatedCStr);
}

NativePath NativePath::fileNamePath() const
{
    return String::fileNamePath(DIR_SEPARATOR);
}

NativePath NativePath::expand(bool* didExpand) const
{
    if(first() == '>' || first() == '}')
    {
        if(didExpand) *didExpand = true;
        return App::app().nativeBasePath() / mid(1);
    }
#ifdef UNIX
    else if(first() == '~')
    {
        if(didExpand) *didExpand = true;

        int firstSlash = indexOf('/');
        if(firstSlash > 1)
        {
            // Parse the user's home directory (from passwd).
            QByteArray userName = left(firstSlash - 1).toLatin1();
            struct passwd* pw = getpwnam(userName);
            if(!pw)
            {
                /// @throws UnknownUserError  User is not known.
                throw UnknownUserError("NativePath::expand",
                                       String("Unknown user '%1'").arg(QLatin1String(userName)));
            }

            return NativePath(pw->pw_dir) / mid(firstSlash);
        }

        // Replace with the HOME path.
        return App::app().nativeHomePath() / mid(1);
    }
#endif

    // No expansion done.
    if(didExpand) *didExpand = false;
    return *this;
}

NativePath NativePath::workPath()
{
    return QDir::currentPath();
}

}
