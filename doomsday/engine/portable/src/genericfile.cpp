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

de::GenericFile::GenericFile(DFile& file, char const* path, LumpInfo const& info)
    : AbstractFile(FT_GENERICFILE, path, file, info)
{}

de::GenericFile::~GenericFile()
{
    F_ReleaseFile(reinterpret_cast<abstractfile_s*>(this));
}

LumpInfo const* de::GenericFile::lumpInfo(int /*lumpIdx*/)
{
    // Generic files are special cases for this *is* the lump.
    return info();
}

int de::GenericFile::lumpCount()
{
    return 1; // Always.
}

int de::GenericFile::publishLumpsToDirectory(LumpDirectory* directory)
{
    LOG_AS("GenericFile");
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
    (inst) != 0? reinterpret_cast<de::GenericFile*>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<de::GenericFile const*>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    de::GenericFile* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    de::GenericFile const* self = TOINTERNAL_CONST(inst)

GenericFile* GenericFile_New(DFile* file, char const* path, LumpInfo const* info)
{
    if(!info) LegacyCore_FatalError("GenericFile_New: Received invalid LumpInfo (=NULL).");
    try
    {
        return reinterpret_cast<GenericFile*>(new de::GenericFile(*reinterpret_cast<de::DFile*>(file), path, *info));
    }
    catch(de::Error& er)
    {
        QString msg = QString("GenericFile_New: Failed to instantiate new GenericFile. ") + er.asText();
        LegacyCore_FatalError(msg.toUtf8().constData());
        exit(1); // Unreachable.
    }
}

void GenericFile_Delete(GenericFile* lump)
{
    if(lump)
    {
        SELF(lump);
        delete self;
    }
}

LumpInfo const* GenericFile_LumpInfo(GenericFile* lump, int lumpIdx)
{
    SELF(lump);
    return self->lumpInfo(lumpIdx);
}

int GenericFile_LumpCount(GenericFile* lump)
{
    SELF(lump);
    return self->lumpCount();
}

int GenericFile_PublishLumpsToDirectory(GenericFile* lump, struct lumpdirectory_s* directory)
{
    SELF(lump);
    return self->publishLumpsToDirectory(reinterpret_cast<de::LumpDirectory*>(directory));
}
