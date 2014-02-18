/**
 * @file unixinfo.cpp
 * Unix system-level configuration.
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/UnixInfo"
#include "de/Info"
#include "de/App"
#include <QDir>

namespace de {
namespace internal {

class Infos
{
    Info *etcInfo;
    Info *userInfo;

public:
    Infos(String fileName) : etcInfo(0), userInfo(0)
    {
        String fn = String("/etc") / App::app().unixEtcFolderName() / fileName;
        if(QFile::exists(fn))
        {
            etcInfo = new Info;
            etcInfo->parseNativeFile(fn);
        }

        fn = String(QDir::homePath()) / App::app().unixHomeFolderName() / fileName;
        if(QFile::exists(fn))
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

    bool find(String const &key, String &value) const
    {
        // User-specific info overrides the system-level info.
        if(userInfo && userInfo->findValueForKey(key, value))
        {
            return true;
        }
        if(etcInfo && etcInfo->findValueForKey(key, value))
        {
            return true;
        }
        return false;
    }
};

} // namespace internal

using namespace internal;

DENG2_PIMPL_NOREF(UnixInfo)
{
    Infos *paths;
    Infos *defaults;

    Instance() : paths(0), defaults(0)
    {}

    ~Instance()
    {
        delete paths;
        delete defaults;
    }
};

UnixInfo::UnixInfo() : d(new Instance)
{
#ifdef UNIX
    /**
     * @note There is only the "paths" and "plugins" config files for now; more
     * could be added for different purposes. There could also be .de scripts
     * for configuration.
     */
    d->paths    = new Infos("paths");
    d->defaults = new Infos("defaults");
#endif
}

bool UnixInfo::path(String const &key, NativePath &value) const
{
    if(d->paths)
    {
        String s;
        if(d->paths->find(key, s))
        {
            value = s;
            return true;
        }
    }
    return false;
}

bool UnixInfo::defaults(String const &key, String &value) const
{
    if(d->defaults)
    {
        return d->defaults->find(key, value);
    }
    return false;
}

} // namespace de
