/**
 * @file abstractfile.cpp
 *
 * Abstract base for all classes which represent loaded files.
 *
 * @ingroup fs
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_filesys.h"

#include "file.h"

namespace de {

File1::File1(FileHandle& hndl, char const* _path, FileInfo const& _info, File1* _container)
    : handle_(&hndl), info_(_info), container_(_container), flags(DefaultFlags)
{
    // Used to favor newer files when duplicates are pruned.
    /// @todo Does not belong at this level. Load order should be determined
    ///       at file system level. -ds
    static uint fileCounter = 0;
    order = fileCounter++;

    Str_Init(&path_); Str_Set(&path_, _path);
}

File1::~File1()
{
    App_FileSystem()->releaseFile(*this);
    Str_Free(&path_);
    if(handle_) delete handle_;
}

FileInfo const& File1::info() const
{
    return info_;
}

bool File1::isContained() const
{
    return !!container_;
}

File1& File1::container() const
{
    if(!container_) throw NotContainedError("File1::container", QString("%s is not contained").arg(Str_Text(path())));
    return *container_;
}

de::FileHandle& File1::handle()
{
    return *handle_;
}

ddstring_t const* File1::path() const
{
    return &path_;
}

uint File1::loadOrderIndex() const
{
    return order;
}

bool File1::hasStartup() const
{
    return flags.testFlag(Startup);
}

File1& File1::setStartup(bool yes)
{
    if(yes) flags |= Startup;
    else    flags &= ~Startup;
    return *this;
}

bool File1::hasCustom() const
{
    return flags.testFlag(Custom);
}

File1& File1::setCustom(bool yes)
{
    if(yes) flags |= Custom;
    else    flags &= ~Custom;
    return *this;
}

ddstring_t const* File1::lumpName(int /*lumpIdx*/)
{
    /// @todo writeme
    throw de::Error("File1::lumpName", "Not yet implemented");
}

AutoStr* File1::composeLumpPath(int /*lumpIdx*/, char /*delimiter*/)
{
    return AutoStr_NewStd();
}

size_t File1::readLump(int /*lumpIdx*/, uint8_t* /*buffer*/, bool /*tryCache*/)
{
    /// @todo writeme
    throw de::Error("File1::readLump", "Not yet implemented");
}

size_t File1::readLump(int /*lumpIdx*/, uint8_t* /*buffer*/, size_t /*startOffset*/,
    size_t /*length*/, bool /*tryCache*/)
{
    /// @todo writeme
    throw de::Error("File1::readLump", "Not yet implemented");
}

uint8_t const* File1::cacheLump(int /*lumpIdx*/)
{
    /// @todo writeme
    throw de::Error("File1::cacheLump", "Not yet implemented");
}

File1& File1::unlockLump(int /*lumpIdx*/)
{
    /// @todo writeme
    throw de::Error("File1::unlockLump", "Not yet implemented");
}

} // namespace de
