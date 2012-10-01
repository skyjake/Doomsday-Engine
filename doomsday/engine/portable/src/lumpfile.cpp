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

de::LumpFile::LumpFile(DFile& file, char const* path, LumpInfo const& info)
    : AbstractFile(FT_LUMPFILE, path, file, info)
{}

de::LumpFile::~LumpFile()
{
    F_ReleaseFile(reinterpret_cast<abstractfile_s*>(this));
}

LumpInfo const* de::LumpFile::lumpInfo(int /*lumpIdx*/)
{
    // Lump files are special cases for this *is* the lump.
    return info();
}

int de::LumpFile::lumpCount()
{
    return 1; // Always.
}

int de::LumpFile::publishLumpsToDirectory(LumpDirectory* directory)
{
    LOG_AS("LumpFile");
    if(directory)
    {
        // This *is* the lump, so insert ourself in the directory.
        AbstractFile* container = reinterpret_cast<de::AbstractFile*>(info()->container);
        if(container)
        {
            directory->catalogLumps(*container, info()->lumpIdx, 1);
        }
    }
    return 1;
}

/**
 * C Wrapper API:
 */

#define TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<de::LumpFile*>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<de::LumpFile const*>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    de::LumpFile* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    de::LumpFile const* self = TOINTERNAL_CONST(inst)

LumpFile* LumpFile_New(DFile* file, char const* path, LumpInfo const* info)
{
    if(!info) LegacyCore_FatalError("LumpFile_New: Received invalid LumpInfo (=NULL).");
    try
    {
        return reinterpret_cast<LumpFile*>(new de::LumpFile(*reinterpret_cast<de::DFile*>(file), path, *info));
    }
    catch(de::Error& er)
    {
        QString msg = QString("LumpFile_New: Failed to instantiate new LumpFile. ") + er.asText();
        LegacyCore_FatalError(msg.toUtf8().constData());
        exit(1); // Unreachable.
    }
}

void LumpFile_Delete(LumpFile* lump)
{
    if(lump)
    {
        SELF(lump);
        delete self;
    }
}

LumpInfo const* LumpFile_LumpInfo(LumpFile* lump, int lumpIdx)
{
    SELF(lump);
    return self->lumpInfo(lumpIdx);
}

int LumpFile_LumpCount(LumpFile* lump)
{
    SELF(lump);
    return self->lumpCount();
}

int LumpFile_PublishLumpsToDirectory(LumpFile* lump, struct lumpdirectory_s* directory)
{
    SELF(lump);
    return self->publishLumpsToDirectory(reinterpret_cast<de::LumpDirectory*>(directory));
}
