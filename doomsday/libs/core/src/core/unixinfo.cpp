/**
 * @file unixinfo.cpp
 * Unix system-level configuration.
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

#include "de/unixinfo.h"
#include "de/info.h"
#include "de/app.h"

namespace de {
namespace internal {

class Infos
{
    Info *etcInfo  = nullptr;
    Info *userInfo = nullptr;

public:
    Infos(const String& fileName)
    {
        String fn = String("/etc") / App::app().unixEtcFolderName() / fileName;
        if (NativePath::exists(fn))
        {
            etcInfo = new Info;
            etcInfo->parseNativeFile(fn);
        }

        fn = NativePath::homePath() / App::app().unixHomeFolderName() / fileName;
        if (NativePath::exists(fn))
        {
            userInfo = new Info;
            userInfo->parseNativeFile(fn);
        }
    }

    ~Infos()
    {
        delete userInfo;
        delete etcInfo;
    }

    bool find(const String &key, String &value) const
    {
        // User-specific info overrides the system-level info.
        if (userInfo && userInfo->findValueForKey(key, value))
        {
            return true;
        }
        if (etcInfo && etcInfo->findValueForKey(key, value))
        {
            return true;
        }
        return false;
    }
};

} // namespace internal

using namespace internal;

DE_PIMPL_NOREF(UnixInfo)
{
    Infos *paths;
    Infos *defaults;

    Impl() : paths(0), defaults(0)
    {}

    ~Impl()
    {
        delete paths;
        delete defaults;
    }
};

UnixInfo::UnixInfo() : d(new Impl)
{
#if defined (UNIX) && !defined(__CYGWIN__)
    /**
     * @note There is only the "paths" and "plugins" config files for now; more
     * could be added for different purposes. There could also be .ds scripts
     * for configuration.
     */
    d->paths    = new Infos("paths");
    d->defaults = new Infos("defaults");
#endif
}

bool UnixInfo::path(const String &key, NativePath &value) const
{
    if (d->paths)
    {
        String s;
        if (d->paths->find(key, s))
        {
            value = NativePath(s).expand();
            return true;
        }
    }
    return false;
}

bool UnixInfo::defaults(const String &key, String &value) const
{
    if (d->defaults)
    {
        return d->defaults->find(key, value);
    }
    return false;
}

} // namespace de
