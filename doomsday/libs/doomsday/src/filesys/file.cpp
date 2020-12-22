/** @file file.cpp
 *
 * Abstract base for all classes which represent loaded files.
 * @ingroup fs
 *
 * @authors Copyright &copy; 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/filesys/file.h"
#include "doomsday/filesys/fs_main.h"

#include <de/nativepath.h>

namespace res {

File1::File1(FileHandle *hndl, String _path, const FileInfo &_info, File1 *_container)
    : handle_(hndl)
    , info_(_info)
    , container_(_container)
    , flags(DefaultFlags)
    , path_(_path)
    , name_(_path.fileName())
{
    // Used to favor newer files when duplicates are pruned.
    /// @todo Does not belong at this level. Load order should be determined
    ///       at file system level. -ds
    static uint fileCounter = 0;
    order = fileCounter++;
}

File1::~File1()
{
    App_FileSystem().releaseFile(*this);
    if (handle_) delete handle_;
}

const FileInfo &File1::info() const
{
    return info_;
}

bool File1::isContained() const
{
    return !!container_;
}

File1 &File1::container() const
{
    if (!container_)
    {
        throw NotContainedError("File1::container",
                                "File \"" + NativePath(composePath()).pretty() +
                                    " is not contained");
    }
    return *container_;
}

FileHandle &File1::handle()
{
    return *handle_;
}

Uri File1::composeUri(Char delimiter) const
{
    return Uri(path_, RC_NULL, delimiter);
}

uint File1::loadOrderIndex() const
{
    return order;
}

bool File1::hasStartup() const
{
    return flags.testFlag(Startup);
}

File1 &File1::setStartup(bool yes)
{
    if (yes) flags |= Startup;
    else    flags &= ~Startup;
    return *this;
}

bool File1::hasCustom() const
{
    return flags.testFlag(Custom);
}

File1 &File1::setCustom(bool yes)
{
    if (yes) flags |= Custom;
    else    flags &= ~Custom;
    return *this;
}

const String &File1::name() const
{
    return name_;
}

size_t File1::read(uint8_t* /*buffer*/, bool /*tryCache*/)
{
    /// @todo writeme
    throw Error("File1::read", "Not yet implemented");
}

size_t File1::read(uint8_t* /*buffer*/, size_t /*startOffset*/, size_t /*length*/,
                   bool /*tryCache*/)
{
    /// @todo writeme
    throw Error("File1::read", "Not yet implemented");
}

const uint8_t *File1::cache()
{
    /// @todo writeme
    throw Error("File1::cache", "Not yet implemented");
}

File1& File1::unlock()
{
    /// @todo writeme
    throw Error("File1::unlock", "Not yet implemented");
}

File1 &File1::clearCache(bool* /*retCleared*/)
{
    /// @todo writeme
    throw Error("File1::clearCache", "Not yet implemented");
}

} // namespace res
