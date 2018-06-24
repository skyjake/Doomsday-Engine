/**
 * @file nativepath.cpp
 * File paths for the native file system.
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/NativePath"
#include "de/App"

#include <c_plus/fileinfo.h>
#include <c_plus/path.h>

/**
 * @def NATIVE_BASE_SYMBOLIC
 *
 * Symbol used in "pretty" paths to represent the base directory (the one set
 * with -basedir).
 */
#define NATIVE_BASE_SYMBOLIC    "(basedir)"

/**
 * @def NATIVE_HOME_SYMBOLIC
 *
 * Symbol used in "pretty" paths to represent the user's native home directory.
 * Note that this is not the same thing as App::nativeHomePath(), which returns
 * the native location of the Doomsday runtime "/home" path where runtime files
 * are stored.
 */

#ifdef WIN32
#  define NATIVE_HOME_SYMBOLIC  "%HOMEPATH%"
#  define DIR_SEPARATOR         '\\'
#else
#  define NATIVE_HOME_SYMBOLIC  "~"
#  define DIR_SEPARATOR         '/'
#  ifdef UNIX
#    include <sys/types.h>
#    include <pwd.h>
#  endif
#endif

namespace de {

static String toNative(String const &s)
{
    // This will resolve parent references (".."), multiple separators
    // (hello//world), and self-references (".").
    String cleaned(s);
    clean_Path(cleaned);
    return Path::normalizeString(cleaned, DIR_SEPARATOR);
}

NativePath::NativePath() : Path()
{}

NativePath::NativePath(NativePath const &other)
    : Path(other)
{}

NativePath::NativePath(NativePath &&moved)
    : Path(moved)
{}

NativePath::NativePath(String const &str) : Path(toNative(str), DIR_SEPARATOR)
{}

NativePath::NativePath(char const *nullTerminatedCStr)
    : Path(toNative(nullTerminatedCStr), DIR_SEPARATOR)
{}

NativePath::NativePath(char const *cStr, dsize length)
    : Path(toNative(String(cStr, length)), DIR_SEPARATOR)
{}

NativePath &NativePath::operator=(String const &str)
{
    set(toNative(str), DIR_SEPARATOR);
    return *this;
}

NativePath &NativePath::operator=(NativePath &&moved)
{
    Path::operator = (moved);
    return *this;
}

NativePath &NativePath::operator=(NativePath const &other)
{
    Path::operator = (other);
    return *this;
}

NativePath &NativePath::operator=(char const *nullTerminatedCStr)
{
    return (*this) = String(nullTerminatedCStr);
}

NativePath NativePath::concatenatePath(NativePath const &nativePath) const
{
    if (nativePath.isAbsolute()) return nativePath;
    return toString().concatenatePath(nativePath, Char(DIR_SEPARATOR));
}

NativePath NativePath::concatenatePath(String const &nativePath) const
{
    return concatenatePath(NativePath(nativePath));
}

NativePath NativePath::operator/(NativePath const &nativePath) const
{
    return concatenatePath(nativePath);
}

NativePath NativePath::operator/(String const &str) const
{
    return *this / NativePath(str);
}

NativePath NativePath::operator/(char const *nullTerminatedCStr) const
{
    return *this / NativePath(nullTerminatedCStr);
}

NativePath NativePath::fileNamePath() const
{
    return NativePath(toString().fileNamePath(DIR_SEPARATOR));
}

bool NativePath::isAbsolute() const
{
    return isAbsolute_Path(expand().toString());
}

bool NativePath::isDirectory() const
{
    cplus::ref<iFileInfo> i(new_FileInfo(toString()));
    return exists_FileInfo(i) && isDirectory_FileInfo(i);
}

NativePath NativePath::expand(bool *didExpand) const
{
    if (first() == '>' || first() == '}')
    {
        if (didExpand) *didExpand = true;
        return App::app().nativeBasePath() / toString().substr(String::BytePos(1));
    }
#ifdef UNIX
    else if (first() == '~')
    {
        if (didExpand) *didExpand = true;

        String const path = toString();
        auto firstSlash = path.indexOf('/');
        if (firstSlash > 1)
        {
            // Parse the user's home directory (from passwd).
            String userName = path.substr(String::BytePos(1), (firstSlash - 1).index);
            struct passwd *pw = getpwnam(userName);
            if (!pw)
            {
                /// @throws UnknownUserError  User is not known.
                throw UnknownUserError("NativePath::expand",
                                       stringf("Unknown user '%s'", userName.c_str()));
            }
            return NativePath(pw->pw_dir) / path.substr(firstSlash + 1);
        }
        else
        {
            // Replace with the HOME path.
            return NativePath::homePath() / path.substr(String::BytePos(2));
        }
    }
#endif

    // No expansion done.
    if (didExpand) *didExpand = false;
    return *this;
}

String NativePath::pretty() const
{
    if (isEmpty()) return *this;

    String result = *this;

    // Replace relative directives like '}' (used in FS1 only) with a full symbol.
    if (result.size() > 1 && (result.first() == '}' || result.first() == '>'))
    {
        return CString(NATIVE_BASE_SYMBOLIC) + DIR_SEPARATOR + result.substr(String::BytePos(1));
    }

    // If within one of the known native directories, cut out the known path,
    // replacing it with a symbolic. This retains the absolute nature of the path
    // while omitting potentially redundant/verbose information.
    if (isAbsolute_Path(result))
    {
        NativePath basePath = App::app().nativeBasePath();
        if (result.beginsWith(basePath.toString()))
        {
            result = NATIVE_BASE_SYMBOLIC + result.substr(basePath.sizeb());
        }
        else
        {
#ifdef MACOSX
            NativePath contentsPath = App::app().nativeAppContentsPath();
            if (result.beginsWith(contentsPath.toString()))
            {
                return "(app)" + result.substr(contentsPath.sizeb());
            }
#endif
#ifndef WIN32 // Windows users are not familiar with a symbolic home path.
            NativePath homePath = NativePath::homePath(); // actual native home dir, not FS2 "/home"
            if (result.beginsWith(homePath.toString()))
            {
                result = NATIVE_HOME_SYMBOLIC + result.substr(homePath.sizeb());
            }
#endif
        }
    }

    return result;
}

String NativePath::withSeparators(Char sep) const
{
    return Path::withSeparators(sep);
}

bool NativePath::exists() const
{
    if (isEmpty()) return false;
    return fileExistsCStr_FileInfo(c_str());
}

bool NativePath::isReadable() const
{
    return exists();
    //return QFileInfo(toString()).isReadable();
}

static std::unique_ptr<NativePath> currentNativeWorkPath;

NativePath NativePath::workPath()
{
    if (!currentNativeWorkPath)
    {
        currentNativeWorkPath.reset(new NativePath(String::take(cwd_Path())));
    }
    return *currentNativeWorkPath;
}

bool NativePath::setWorkPath(const NativePath &cwd)
{
    if (setCwd_Path(cwd.toString()))
    {
        currentNativeWorkPath.reset(new NativePath(cwd));
        return true;
    }
    return false;
}

NativePath NativePath::homePath()
{
#if defined (UNIX)
    return getenv("HOME");
#elif defined (WIN32)
    return getenv("HOMEPATH");
#endif
}

bool NativePath::exists(NativePath const &nativePath)
{
    return fileExistsCStr_FileInfo(nativePath);
}

void NativePath::createPath(NativePath const &nativePath) // static
{
    NativePath parentPath = nativePath.fileNamePath();
    if (!parentPath.isEmpty() && !exists(parentPath))
    {
        parentPath.create();
    }

    mkdir_Path(nativePath.toString());

    if (!nativePath.exists())
    {
        /// @throw CreateDirError Failed to create directory @a nativePath.
        throw CreateDirError("NativePath::createDirectory", "Could not create: " + nativePath);
    }
}

bool NativePath::destroyPath(const NativePath &nativePath) // static
{
    if (!nativePath.isEmpty())
    {
        if (nativePath.isDirectory())
        {
            return QDir::current().rmdir(nativePath);
        }
        else
        {
            return remove(nativePath.c_str()) == 0;

    }
    return true;
}

Char NativePath::separator()
{
    return DIR_SEPARATOR;
}

} // namespace de
