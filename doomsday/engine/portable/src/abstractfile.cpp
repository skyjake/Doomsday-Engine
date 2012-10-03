/**
 * @file abstractfile.cpp
 *
 * Abstract base for all classes which represent opened files.
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

#include "abstractfile.h"

de::AbstractFile::AbstractFile(filetype_t _type, char const* _path, DFile& file, LumpInfo const& _info)
    : type_(_type), file(&file)
{
    // Used to favor newer files when duplicates are pruned.
    static uint fileCounter = 0;
    DENG2_ASSERT(VALID_FILETYPE(_type));
    order = fileCounter++;

    flags.startup = false;
    flags.custom = true;
    Str_Init(&path_); Str_Set(&path_, _path);
    F_CopyLumpInfo(&info_, &_info);
}

de::AbstractFile::~AbstractFile()
{
    Str_Free(&path_);
    F_DestroyLumpInfo(&info_);
    if(file) delete file;
}

filetype_t de::AbstractFile::type() const
{
    return type_;
}

LumpInfo const* de::AbstractFile::info() const
{
    return &info_;
}

de::AbstractFile* de::AbstractFile::container() const
{
    return reinterpret_cast<de::AbstractFile*>(info_.container);
}

size_t de::AbstractFile::baseOffset() const
{
    return (file? file->baseOffset() : 0);
}

de::DFile* de::AbstractFile::handle()
{
    return file;
}

ddstring_t const* de::AbstractFile::path() const
{
    return &path_;
}

uint de::AbstractFile::loadOrderIndex() const
{
    return order;
}

uint de::AbstractFile::lastModified() const
{
    return info_.lastModified;
}

bool de::AbstractFile::hasStartup() const
{
    return !!flags.startup;
}

void de::AbstractFile::setStartup(bool yes)
{
    flags.startup = yes;
}

bool de::AbstractFile::hasCustom() const
{
    return !!flags.custom;
}

void de::AbstractFile::setCustom(bool yes)
{
    flags.custom = yes;
}

/**
 * C Wrapper API:
 */

#define TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<de::AbstractFile*>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<de::AbstractFile const*>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    de::AbstractFile* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    de::AbstractFile const* self = TOINTERNAL_CONST(inst)

filetype_t AbstractFile_Type(AbstractFile const* af)
{
    SELF_CONST(af);
    return self->type();
}

LumpInfo const* AbstractFile_Info(AbstractFile const* af)
{
    SELF_CONST(af);
    return self->info();
}

AbstractFile* AbstractFile_Container(AbstractFile const* af)
{
    SELF_CONST(af);
    return reinterpret_cast<AbstractFile*>(self->container());
}

size_t AbstractFile_BaseOffset(AbstractFile const* af)
{
    SELF_CONST(af);
    return self->baseOffset();
}

DFile* AbstractFile_Handle(AbstractFile* af)
{
    SELF(af);
    return reinterpret_cast<DFile*>(self->handle());
}

ddstring_t const* AbstractFile_Path(AbstractFile const* af)
{
    SELF_CONST(af);
    return self->path();
}

uint AbstractFile_LoadOrderIndex(AbstractFile const* af)
{
    SELF_CONST(af);
    return self->loadOrderIndex();
}

uint AbstractFile_LastModified(AbstractFile const* af)
{
    SELF_CONST(af);
    return self->lastModified();
}

boolean AbstractFile_HasStartup(AbstractFile const* af)
{
    SELF_CONST(af);
    return self->hasStartup();
}

void AbstractFile_SetStartup(AbstractFile* af, boolean yes)
{
    SELF(af);
    self->setStartup(CPP_BOOL(yes));
}

boolean AbstractFile_HasCustom(AbstractFile const* af)
{
    SELF_CONST(af);
    return self->hasCustom();
}

void AbstractFile_SetCustom(AbstractFile* af, boolean yes)
{
    SELF(af);
    self->setCustom(CPP_BOOL(yes));
}
