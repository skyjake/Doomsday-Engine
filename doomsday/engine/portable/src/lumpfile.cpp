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

#include "de_base.h"
#include "de_filesys.h"

#include "lumpfile.h"
#include "lumpdirectory.h"

#include <de/Error>
#include <de/Log>
#include <de/memory.h>

using namespace de;
using de::AbstractFile;
using de::DFile;
using de::LumpFile;
using de::PathDirectoryNode;

LumpFile::LumpFile(DFile& file, char const* path, LumpInfo const& info)
    : AbstractFile(FT_LUMPFILE, path, file, info)
{}

LumpFile::~LumpFile()
{}

PathDirectoryNode* LumpFile::lumpDirectoryNode(int lumpIdx)
{
    return container()->lumpDirectoryNode(lumpInfo(lumpIdx)->lumpIdx);
}

AutoStr* LumpFile::composeLumpPath(int lumpIdx, char delimiter)
{
    return container()->composeLumpPath(lumpInfo(lumpIdx)->lumpIdx, delimiter);
}

LumpInfo const* LumpFile::lumpInfo(int /*lumpIdx*/)
{
    // Lump files are special cases for this *is* the lump.
    return info();
}

size_t LumpFile::lumpSize(int lumpIdx)
{
    // Lump files are special cases for this *is* the lump.
    return lumpInfo(lumpIdx)->size;
}

size_t LumpFile::readLump(int lumpIdx, uint8_t* buffer, bool tryCache)
{
    return container()->readLump(lumpInfo(lumpIdx)->lumpIdx, buffer, tryCache);
}

size_t LumpFile::readLump(int lumpIdx, uint8_t* buffer, size_t startOffset,
    size_t length, bool tryCache)
{
    return container()->readLump(lumpInfo(lumpIdx)->lumpIdx, buffer,
                                 startOffset, length, tryCache);
}

uint8_t const* LumpFile::cacheLump(int lumpIdx)
{
    return container()->cacheLump(lumpInfo(lumpIdx)->lumpIdx);
}

LumpFile& LumpFile::unlockLump(int lumpIdx)
{
    container()->unlockLump(lumpInfo(lumpIdx)->lumpIdx);
    return *this;
}

int LumpFile::publishLumpsToDirectory(LumpDirectory* directory)
{
    LOG_AS("LumpFile");
    if(directory)
    {
        // This *is* the lump, so insert ourself as a lump of our container in the directory.
        AbstractFile* container = reinterpret_cast<AbstractFile*>(info()->container);
        DENG_ASSERT(container);
        directory->catalogLumps(*container, info()->lumpIdx, 1);
    }
    return 1;
}

/**
 * C Wrapper API:
 */

#define TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<LumpFile*>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<LumpFile const*>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    LumpFile* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    LumpFile const* self = TOINTERNAL_CONST(inst)

struct lumpfile_s* LumpFile_New(struct dfile_s* hndl, char const* path, LumpInfo const* info)
{
    if(!info) LegacyCore_FatalError("LumpFile_New: Received invalid LumpInfo (=NULL).");
    try
    {
        return reinterpret_cast<struct lumpfile_s*>(new LumpFile(*reinterpret_cast<DFile*>(hndl), path, *info));
    }
    catch(Error& er)
    {
        QString msg = QString("LumpFile_New: Failed to instantiate new LumpFile. ") + er.asText();
        LegacyCore_FatalError(msg.toUtf8().constData());
        exit(1); // Unreachable.
    }
}

void LumpFile_Delete(struct lumpfile_s* lump)
{
    if(lump)
    {
        SELF(lump);
        delete self;
    }
}

struct pathdirectorynode_s* LumpFile_LumpDirectoryNode(struct lumpfile_s* lump, int lumpIdx)
{
    SELF(lump);
    return reinterpret_cast<struct pathdirectorynode_s*>( self->lumpDirectoryNode(lumpIdx) );
}

AutoStr* LumpFile_ComposeLumpPath(struct lumpfile_s* lump, int lumpIdx, char delimiter)
{
    SELF(lump);
    return self->composeLumpPath(lumpIdx, delimiter);
}

LumpInfo const* LumpFile_LumpInfo(struct lumpfile_s* lump, int lumpIdx)
{
    SELF(lump);
    return self->lumpInfo(lumpIdx);
}

int LumpFile_LumpCount(struct lumpfile_s* lump)
{
    SELF(lump);
    return self->lumpCount();
}

size_t LumpFile_ReadLumpSection2(struct lumpfile_s* lump, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length, boolean tryCache)
{
    SELF(lump);
    return self->readLump(lumpIdx, buffer, startOffset, length, CPP_BOOL(tryCache));
}

size_t LumpFile_ReadLumpSection(struct lumpfile_s* lump, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length)
{
    SELF(lump);
    return self->readLump(lumpIdx, buffer, startOffset, length);
}

size_t LumpFile_ReadLump2(struct lumpfile_s* lump, int lumpIdx, uint8_t* buffer, boolean tryCache)
{
    SELF(lump);
    return self->readLump(lumpIdx, buffer, CPP_BOOL(tryCache));
}

size_t LumpFile_ReadLump(struct lumpfile_s* lump, int lumpIdx, uint8_t* buffer)
{
    SELF(lump);
    return self->readLump(lumpIdx, buffer);
}

uint8_t const* LumpFile_CacheLump(struct lumpfile_s* lump, int lumpIdx)
{
    SELF(lump);
    return self->cacheLump(lumpIdx);
}

void LumpFile_UnlockLump(struct lumpfile_s* lump, int lumpIdx)
{
    SELF(lump);
    self->unlockLump(lumpIdx);
}

int LumpFile_PublishLumpsToDirectory(struct lumpfile_s* lump, struct lumpdirectory_s* directory)
{
    SELF(lump);
    return self->publishLumpsToDirectory(reinterpret_cast<LumpDirectory*>(directory));
}
