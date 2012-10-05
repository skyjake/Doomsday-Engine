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

using de::DFile;
using de::AbstractFile;

AbstractFile::AbstractFile(filetype_t _type, char const* _path, DFile& file, LumpInfo const& _info)
    : file(&file), type_(_type)
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

AbstractFile::~AbstractFile()
{
    FS::releaseFile(this);
    Str_Free(&path_);
    F_DestroyLumpInfo(&info_);
    if(file) delete file;
}

filetype_t AbstractFile::type() const
{
    return type_;
}

LumpInfo const* AbstractFile::info() const
{
    return &info_;
}

bool AbstractFile::isContained() const
{
    return !!info_.container;
}

AbstractFile& AbstractFile::container() const
{
    AbstractFile* cont = reinterpret_cast<AbstractFile*>(info_.container);
    if(!cont) throw de::Error("AbstractFile::container", QString("%s is not contained").arg(Str_Text(path())));
    return *cont;
}

size_t AbstractFile::baseOffset() const
{
    return (file? file->baseOffset() : 0);
}

DFile* AbstractFile::handle()
{
    return file;
}

ddstring_t const* AbstractFile::path() const
{
    return &path_;
}

uint AbstractFile::loadOrderIndex() const
{
    return order;
}

uint AbstractFile::lastModified() const
{
    return info_.lastModified;
}

bool AbstractFile::hasStartup() const
{
    return !!flags.startup;
}

AbstractFile& AbstractFile::setStartup(bool yes)
{
    flags.startup = yes;
    return *this;
}

bool AbstractFile::hasCustom() const
{
    return !!flags.custom;
}

AbstractFile& AbstractFile::setCustom(bool yes)
{
    flags.custom = yes;
    return *this;
}

/**
 * C Wrapper API:
 */

#define TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<AbstractFile*>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<AbstractFile const*>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    AbstractFile* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    AbstractFile const* self = TOINTERNAL_CONST(inst)

filetype_t AbstractFile_Type(struct abstractfile_s const* af)
{
    SELF_CONST(af);
    return self->type();
}

LumpInfo const* AbstractFile_Info(struct abstractfile_s const* af)
{
    SELF_CONST(af);
    return self->info();
}

boolean AbstractFile_IsContained(struct abstractfile_s const* af)
{
    SELF_CONST(af);
    return self->isContained();
}

struct abstractfile_s* AbstractFile_Container(struct abstractfile_s const* af)
{
    SELF_CONST(af);
    return reinterpret_cast<struct abstractfile_s*>(&self->container());
}

size_t AbstractFile_BaseOffset(struct abstractfile_s const* af)
{
    SELF_CONST(af);
    return self->baseOffset();
}

struct dfile_s* AbstractFile_Handle(struct abstractfile_s* af)
{
    SELF(af);
    return reinterpret_cast<struct dfile_s*>(self->handle());
}

ddstring_t const* AbstractFile_Path(struct abstractfile_s const* af)
{
    SELF_CONST(af);
    return self->path();
}

uint AbstractFile_LoadOrderIndex(struct abstractfile_s const* af)
{
    SELF_CONST(af);
    return self->loadOrderIndex();
}

uint AbstractFile_LastModified(struct abstractfile_s const* af)
{
    SELF_CONST(af);
    return self->lastModified();
}

boolean AbstractFile_HasStartup(struct abstractfile_s const* af)
{
    SELF_CONST(af);
    return self->hasStartup();
}

void AbstractFile_SetStartup(struct abstractfile_s* af, boolean yes)
{
    SELF(af);
    self->setStartup(CPP_BOOL(yes));
}

boolean AbstractFile_HasCustom(struct abstractfile_s const* af)
{
    SELF_CONST(af);
    return self->hasCustom();
}

void AbstractFile_SetCustom(struct abstractfile_s* af, boolean yes)
{
    SELF(af);
    self->setCustom(CPP_BOOL(yes));
}
