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

de::AbstractFile& de::AbstractFile::init(filetype_t type, char const* path,
    DFile* file, const LumpInfo* info)
{
    // Used to favor newer files when duplicates are pruned.
    static uint fileCounter = 0;
    DENG2_ASSERT(VALID_FILETYPE(type) && info);

    _order = fileCounter++;
    _type = type;
    _file = file;
    _flags.startup = false;
    _flags.custom = true;
    Str_Init(&_path); Str_Set(&_path, path);
    F_CopyLumpInfo(&_info, info);

    return *this;
}

void de::AbstractFile::destroy()
{
    Str_Free(&_path);
    F_DestroyLumpInfo(&_info);
    if(_file) DFile_Delete(_file, true);
}

filetype_t de::AbstractFile::type() const
{
    return _type;
}

LumpInfo const* de::AbstractFile::info() const
{
    return &_info;
}

de::AbstractFile* de::AbstractFile::container() const
{
    return reinterpret_cast<de::AbstractFile*>(_info.container);
}

size_t de::AbstractFile::baseOffset() const
{
    return (_file? DFile_BaseOffset(_file) : 0);
}

DFile* de::AbstractFile::handle()
{
    return _file;
}

ddstring_t const* de::AbstractFile::path() const
{
    return &_path;
}

uint de::AbstractFile::loadOrderIndex() const
{
    return _order;
}

uint de::AbstractFile::lastModified() const
{
    return _info.lastModified;
}

bool de::AbstractFile::hasStartup() const
{
    return !!_flags.startup;
}

void de::AbstractFile::setStartup(bool yes)
{
    _flags.startup = yes;
}

bool de::AbstractFile::hasCustom() const
{
    return !!_flags.custom;
}

void de::AbstractFile::setCustom(bool yes)
{
    _flags.custom = yes;
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

AbstractFile* AbstractFile_Init(AbstractFile* af, filetype_t type,
    char const* path, DFile* file, const LumpInfo* info)
{
    SELF(af);
    return reinterpret_cast<AbstractFile*>(&self->init(type, path, file, info));
}

void AbstractFile_Destroy(AbstractFile* af)
{
    SELF(af);
    self->destroy();
}

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
    return self->handle();
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

AbstractFile* UnknownFile_New(DFile* file, char const* path, LumpInfo const* info)
{
    de::AbstractFile* af = new de::AbstractFile();
    if(!af) LegacyCore_FatalError("UnknownFile_New: Failed to instantiate new AbstractFile.");
    return reinterpret_cast<AbstractFile*>(&af->init(FT_UNKNOWNFILE, path, file, info));
}
