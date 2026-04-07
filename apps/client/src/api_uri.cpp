/** @file api_uri.cpp Universal Resource Identifier (public C wrapper).
 * @ingroup base
 *
 * @authors Copyright &copy; 2010-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2010-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#define DE_NO_API_MACROS_URI // functions defined here
#include "api_uri.h"

#include <doomsday/uri.h>
#include <doomsday/dualstring.h>
#include <de/legacy/writer.h>
#include <de/legacy/reader.h>

#define TOINTERNAL(inst)        reinterpret_cast<res::Uri*>(inst)
#define TOINTERNAL_CONST(inst)  reinterpret_cast<res::Uri const*>(inst)

#define SELF(inst) \
    DE_ASSERT(inst); \
    res::Uri* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DE_ASSERT(inst); \
    res::Uri const* self = TOINTERNAL_CONST(inst)

static void readUri(Uri *uri, Reader1 *reader, de::String defaultScheme = "")
{
    SELF(uri);
    self->readUri(reader, defaultScheme);
}

static void writeUri(const Uri *uri, Writer1 *writer, int omitComponents = 0)
{
    SELF_CONST(uri);
    self->writeUri(writer, omitComponents);
}

#undef Uri_Clear
Uri* Uri_Clear(Uri* uri)
{
    SELF(uri);
    return reinterpret_cast<Uri*>(&self->clear());
}

#undef Uri_SetScheme
Uri* Uri_SetScheme(Uri* uri, char const* scheme)
{
    SELF(uri);
    return reinterpret_cast<Uri*>(&self->setScheme(scheme));
}

#undef Uri_SetPath
Uri* Uri_SetPath(Uri* uri, char const* path)
{
    SELF(uri);
    return reinterpret_cast<Uri*>(&self->setPath(path));
}

#undef Uri_NewWithPath3
Uri* Uri_NewWithPath3(const char *defaultScheme, const char *path)
{
    res::Uri *uri = new res::Uri(defaultScheme);
    uri->setUri(path, RC_NULL);
    return reinterpret_cast<Uri *>(uri);
}

#undef Uri_NewWithPath2
Uri* Uri_NewWithPath2(char const* path, resourceclassid_t defaultResourceClass)
{
    return reinterpret_cast<Uri*>( new res::Uri(path, defaultResourceClass) );
}

#undef Uri_NewWithPath
Uri* Uri_NewWithPath(char const* path)
{
    return reinterpret_cast<Uri*>( new res::Uri(path) );
}

#undef Uri_New
Uri* Uri_New(void)
{
    return reinterpret_cast<Uri*>( new res::Uri() );
}

#undef Uri_Dup
Uri* Uri_Dup(Uri const* other)
{
    DE_ASSERT(other);
    return reinterpret_cast<Uri*>( new res::Uri(*(TOINTERNAL_CONST(other))) );
}

#undef Uri_FromReader
Uri* Uri_FromReader(Reader1 *reader)
{
    DE_ASSERT(reader);

    res::Uri* self = new res::Uri;
    Uri* uri = reinterpret_cast<Uri*>(self);

    readUri(uri, reader);

    return uri;
}

#undef Uri_Delete
void Uri_Delete(Uri* uri)
{
    if(uri)
    {
        SELF(uri);
        delete self;
    }
}

#undef Uri_Copy
Uri* Uri_Copy(Uri* uri, Uri const* other)
{
    SELF(uri);
    DE_ASSERT(other);
    *self = *(TOINTERNAL_CONST(other));
    return reinterpret_cast<Uri*>(self);
}

#undef Uri_Equality
dd_bool Uri_Equality(Uri const* uri, Uri const* other)
{
    SELF_CONST(uri);
    DE_ASSERT(other);
    return *self == (*(TOINTERNAL_CONST(other)));
}

#undef Uri_IsEmpty
dd_bool Uri_IsEmpty(Uri const* uri)
{
    SELF_CONST(uri);
    return self->isEmpty();
}

#undef Uri_Resolved
AutoStr* Uri_Resolved(Uri const* uri)
{
    SELF_CONST(uri);
    try
    {
        return AutoStr_FromTextStd(self->resolved());
    }
    catch(res::Uri::ResolveError const& er)
    {
        LOG_RES_WARNING(er.asText());
    }
    return AutoStr_NewStd();
}

#undef Uri_Scheme
const Str* Uri_Scheme(Uri const* uri)
{
    SELF_CONST(uri);
    return AutoStr_FromTextStd(self->scheme());
}

#undef Uri_Path
const Str* Uri_Path(Uri const* uri)
{
    SELF_CONST(uri);
    return AutoStr_FromTextStd(self->path());
}

#undef Uri_SetUri2
Uri* Uri_SetUri2(Uri* uri, char const* path, resourceclassid_t defaultResourceClass)
{
    SELF(uri);
    return reinterpret_cast<Uri*>(&self->setUri(path, defaultResourceClass));
}

#undef Uri_SetUri
Uri* Uri_SetUri(Uri* uri, char const* path)
{
    SELF(uri);
    return reinterpret_cast<Uri*>(&self->setUri(path));
}

#undef Uri_SetUriStr
Uri* Uri_SetUriStr(Uri* uri, ddstring_t const* path)
{
    SELF(uri);
    return reinterpret_cast<Uri*>(&self->setUri(Str_Text(path)));
}

static res::Uri::ComposeAsTextFlags translateFlags(int flags)
{
    res::Uri::ComposeAsTextFlags catf;
    if(flags & UCTF_OMITSCHEME) catf |= res::Uri::OmitScheme;
    if(flags & UCTF_OMITPATH)   catf |= res::Uri::OmitPath;
    if(flags & UCTF_DECODEPATH) catf |= res::Uri::DecodePath;
    return catf;
}

#undef Uri_Compose2
AutoStr* Uri_Compose2(Uri const* uri, int flags)
{
    SELF_CONST(uri);
    return AutoStr_FromTextStd(self->compose(translateFlags(flags)));
}

#undef Uri_Compose
AutoStr* Uri_Compose(Uri const* uri)
{
    SELF_CONST(uri);
    return AutoStr_FromTextStd(self->compose());
}

#undef Uri_ToString
AutoStr* Uri_ToString(Uri const* uri)
{
    SELF_CONST(uri);
    return AutoStr_FromTextStd(self->asText());
}

#undef Uri_Write2
void Uri_Write2(Uri const* uri, struct writer_s* writer, int omitComponents)
{
    DE_ASSERT(uri);
    DE_ASSERT(writer);
    writeUri(uri, writer, omitComponents);
}

#undef Uri_Write
void Uri_Write(Uri const* uri, struct writer_s* writer)
{
    DE_ASSERT(uri);
    DE_ASSERT(writer);
    writeUri(uri, writer);
}

#undef Uri_Read
Uri* Uri_Read(Uri* uri, struct reader_s* reader)
{
    DE_ASSERT(uri);
    DE_ASSERT(reader);
    readUri(uri, reader);
    return uri;
}

#undef Uri_ReadWithDefaultScheme
void Uri_ReadWithDefaultScheme(Uri* uri, struct reader_s* reader, char const* defaultScheme)
{
    DE_ASSERT(uri);
    DE_ASSERT(reader);
    readUri(uri, reader, defaultScheme);
}

DE_DECLARE_API(Uri) =
{
    { DE_API_URI },
    Uri_New,
    Uri_NewWithPath3,
    Uri_NewWithPath2,
    Uri_NewWithPath,
    Uri_Dup,
    Uri_FromReader,
    Uri_Delete,
    Uri_IsEmpty,
    Uri_Clear,
    Uri_Copy,
    Uri_Resolved,
    Uri_Scheme,
    Uri_Path,
    Uri_SetScheme,
    Uri_SetPath,
    Uri_SetUri2,
    Uri_SetUri,
    Uri_SetUriStr,
    Uri_Compose2,
    Uri_Compose,
    Uri_ToString,
    Uri_Equality,
    Uri_Write2,
    Uri_Write,
    Uri_Read,
    Uri_ReadWithDefaultScheme
};
