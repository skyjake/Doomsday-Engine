/**
 * @file genericfile.cpp
 *
 * Generic implementation of AbstractFile for use with unknown file types.
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
#include "lumpindex.h"

#include "genericfile.h"

#include <de/Error>
#include <de/Log>
#include <de/memory.h>

namespace de {

GenericFile::GenericFile(DFile& file, char const* path, LumpInfo const& info)
    : AbstractFile(FT_GENERICFILE, path, file, info)
{}

GenericFile::~GenericFile()
{}

PathDirectoryNode const& GenericFile::lumpDirectoryNode(int /*lumpIdx*/)
{
    /// @todo writeme
    throw de::Error("GenericFile::lumpDirectoryNode", "Not yet implemented");
}

AutoStr* GenericFile::composeLumpPath(int /*lumpIdx*/, char /*delimiter*/)
{
    return AutoStr_NewStd();
}

size_t GenericFile::lumpSize(int lumpIdx)
{
    // Generic files are special cases for this *is* the lump.
    return lumpInfo(lumpIdx).size;
}

size_t GenericFile::readLump(int /*lumpIdx*/, uint8_t* /*buffer*/, bool /*tryCache*/)
{
    /// @todo writeme
    throw de::Error("GenericFile::readLump", "Not yet implemented");
}

size_t GenericFile::readLump(int /*lumpIdx*/, uint8_t* /*buffer*/, size_t /*startOffset*/,
    size_t /*length*/, bool /*tryCache*/)
{
    /// @todo writeme
    throw de::Error("GenericFile::readLump", "Not yet implemented");
}

uint8_t const* GenericFile::cacheLump(int /*lumpIdx*/)
{
    /// @todo writeme
    throw de::Error("GenericFile::cacheLump", "Not yet implemented");
}

GenericFile& GenericFile::unlockLump(int /*lumpIdx*/)
{
    /// @todo writeme
    throw de::Error("GenericFile::unlockLump", "Not yet implemented");
}

int GenericFile::publishLumpsToIndex(LumpIndex& index)
{
    LOG_AS("GenericFile");
    // This *is* the lump, so insert ourself in the index.
    index.catalogLumps(*this, 0, 1);
    return 1;
}

} // namespace de
