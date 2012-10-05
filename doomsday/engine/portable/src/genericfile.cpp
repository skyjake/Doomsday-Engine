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

#include "genericfile.h"
#include "lumpdirectory.h"

#include <de/Error>
#include <de/Log>
#include <de/memory.h>

using namespace de;
using de::AbstractFile;
using de::DFile;
using de::GenericFile;
using de::PathDirectoryNode;

GenericFile::GenericFile(DFile& file, char const* path, LumpInfo const& info)
    : AbstractFile(FT_GENERICFILE, path, file, info)
{}

GenericFile::~GenericFile()
{}

PathDirectoryNode* GenericFile::lumpDirectoryNode(int /*lumpIdx*/)
{
    /// @todo writeme
    return 0;
}

AutoStr* GenericFile::composeLumpPath(int /*lumpIdx*/, char /*delimiter*/)
{
    return AutoStr_NewStd();
}

LumpInfo const* GenericFile::lumpInfo(int /*lumpIdx*/)
{
    // Generic files are special cases for this *is* the lump.
    return info();
}

size_t GenericFile::lumpSize(int lumpIdx)
{
    // Generic files are special cases for this *is* the lump.
    return lumpInfo(lumpIdx)->size;
}

size_t GenericFile::readLump(int lumpIdx, uint8_t* buffer, bool tryCache)
{
    /// @todo writeme
    return 0;
}

size_t GenericFile::readLump(int lumpIdx, uint8_t* buffer, size_t startOffset,
    size_t length, bool tryCache)
{
    /// @todo writeme
    return 0;
}

uint8_t const* GenericFile::cacheLump(int /*lumpIdx*/)
{
    /// @todo writeme
    return 0;
}

GenericFile& GenericFile::unlockLump(int /*lumpIdx*/)
{
    /// @todo writeme
    return *this;
}

int GenericFile::publishLumpsToDirectory(LumpDirectory* directory)
{
    LOG_AS("GenericFile");
    if(directory)
    {
        // This *is* the lump, so insert ourself in the directory.
        directory->catalogLumps(*this, 0, 1);
    }
    return 1;
}

/**
 * C Wrapper API:
 */

#define TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<GenericFile*>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<GenericFile const*>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    GenericFile* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    GenericFile const* self = TOINTERNAL_CONST(inst)

struct genericfile_s* GenericFile_New(struct dfile_s* hndl, char const* path, LumpInfo const* info)
{
    if(!info) LegacyCore_FatalError("GenericFile_New: Received invalid LumpInfo (=NULL).");
    try
    {
        return reinterpret_cast<struct genericfile_s*>(new GenericFile(*reinterpret_cast<DFile*>(hndl), path, *info));
    }
    catch(de::Error& er)
    {
        QString msg = QString("GenericFile_New: Failed to instantiate new GenericFile. ") + er.asText();
        LegacyCore_FatalError(msg.toUtf8().constData());
        exit(1); // Unreachable.
    }
}

void GenericFile_Delete(struct genericfile_s* file)
{
    if(file)
    {
        SELF(file);
        delete self;
    }
}

LumpInfo const* GenericFile_LumpInfo(struct genericfile_s* file, int lumpIdx)
{
    SELF(file);
    return self->lumpInfo(lumpIdx);
}

int GenericFile_LumpCount(struct genericfile_s* file)
{
    SELF(file);
    return self->lumpCount();
}

int GenericFile_PublishLumpsToDirectory(struct genericfile_s* file, struct lumpdirectory_s* directory)
{
    SELF(file);
    return self->publishLumpsToDirectory(reinterpret_cast<LumpDirectory*>(directory));
}
