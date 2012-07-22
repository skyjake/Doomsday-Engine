/**
 * @file unixinfo.cpp
 * Unix system-level configuration. @ingroup core
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

#include "de/UnixInfo"
#include "de/Info"
#include <QDir>

using namespace de;

struct UnixInfo::Instance
{
    Info* etcInfo;
    Info* userInfo;

    Instance(String fileName) : etcInfo(0), userInfo(0)
    {
#ifdef UNIX
        String fn = "/etc/doomsday/" + fileName;
        if(QFile::exists(fn))
        {
            etcInfo = new Info;
            etcInfo->parseNativeFile(fn);
        }

        fn = QDir::homePath() + "/.doomsday/" + fileName;
        if(QFile::exists(fn))
        {
            userInfo = new Info;
            userInfo->parseNativeFile(fn);
        }
#else
        DENG2_UNUSED(fileName);
#endif
    }

    ~Instance()
    {
        delete userInfo;
        delete etcInfo;
    }
};

UnixInfo::UnixInfo()
{
    /**
     * @note There is only the "paths" config file for now; more could be added
     * for different purposes. There could also be .de scripts for
     * configuration.
     */
    d = new Instance("paths");
}

UnixInfo::~UnixInfo()
{
    delete d;
}

bool UnixInfo::path(const String& key, String& value) const
{
    // User-specific info overrides the system-level info.
    if(d->userInfo && d->userInfo->findValueForKey(key, value))
    {
        return true;
    }

    if(d->etcInfo && d->etcInfo->findValueForKey(key, value))
    {
        return true;
    }

    return false;
}
