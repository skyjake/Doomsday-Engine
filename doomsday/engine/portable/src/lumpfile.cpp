/**
 * @file lumpfile.cpp
 * Lump (file) accessor abstraction for containers. @ingroup fs
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

#include "lumpindex.h"
#include "lumpfile.h"

using namespace de;
using de::DFile;
using de::PathDirectoryNode;

LumpFile::LumpFile(DFile& file, char const* path, LumpInfo const& info)
    : AbstractFile(FT_LUMPFILE, path, file, info)
{}

LumpFile::~LumpFile()
{}

PathDirectoryNode const& LumpFile::lumpDirectoryNode(int /*lumpIdx*/)
{
    // Lump files are special cases for this *is* the lump.
    return container().lumpDirectoryNode(info().lumpIdx);
}

AutoStr* LumpFile::composeLumpPath(int /*lumpIdx*/, char delimiter)
{
    // Lump files are special cases for this *is* the lump.
    return container().composeLumpPath(info().lumpIdx, delimiter);
}

size_t LumpFile::lumpSize(int /*lumpIdx*/)
{
    // Lump files are special cases for this *is* the lump.
    return info().size;
}

size_t LumpFile::readLump(int /*lumpIdx*/, uint8_t* buffer, bool tryCache)
{
    // Lump files are special cases for this *is* the lump.
    return container().readLump(info().lumpIdx, buffer, tryCache);
}

size_t LumpFile::readLump(int /*lumpIdx*/, uint8_t* buffer, size_t startOffset,
    size_t length, bool tryCache)
{
    // Lump files are special cases for this *is* the lump.
    return container().readLump(info().lumpIdx, buffer, startOffset, length, tryCache);
}

uint8_t const* LumpFile::cacheLump(int /*lumpIdx*/)
{
    // Lump files are special cases for this *is* the lump.
    return container().cacheLump(info().lumpIdx);
}

LumpFile& LumpFile::unlockLump(int /*lumpIdx*/)
{
    // Lump files are special cases for this *is* the lump.
    container().unlockLump(info().lumpIdx);
    return *this;
}

int LumpFile::publishLumpsToIndex(LumpIndex& index)
{
    // This *is* the lump, so insert ourself as a lump of our container in the index.
    index.catalogLumps(container(), info().lumpIdx, 1);
    return 1;
}
