/**
 * @file uri_wrapper.cpp
 * Universal Resource Identifier (C wrapper). @ingroup base
 *
 * @author Copyright &copy; 2010-2012 Daniel Swanson <danij@dengine.net>
 * @author Copyright &copy; 2010-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#define DENG_NO_API_MACROS_URI // functions defined here
#include "api_uri.h"

#include "uri.hh"
#include "dualstring.h"
#include <de/writer.h>
#include <de/reader.h>

#define TOINTERNAL(inst)        reinterpret_cast<de::Uri*>(inst)
#define TOINTERNAL_CONST(inst)  reinterpret_cast<de::Uri const*>(inst)

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    de::Uri* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    de::Uri const* self = TOINTERNAL_CONST(inst)

static void writeUri(const Uri* uri, Writer* writer, int omitComponents = 0)
{
    SELF_CONST(uri);

    if(omitComponents & UCF_SCHEME)
    {
        ddstring_t emptyString;
        Str_InitStatic(&emptyString, "");
        Str_Write(&emptyString, writer);
    }
    else
    {
        Str_Write(de::DualString(self->scheme()).toStrUtf8(), writer);
    }
    Str_Write(de::DualString(self->path()).toStrUtf8(), writer);
}

Uri* Uri_Clear(Uri* uri)
{
    SELF(uri);
    return reinterpret_cast<Uri*>(&self->clear());
}

Uri* Uri_SetScheme(Uri* uri, char const* scheme)
{
    SELF(uri);
    return reinterpret_cast<Uri*>(&self->setScheme(scheme));
}

Uri* Uri_SetPath(Uri* uri, char const* path)
{
    SELF(uri);
    return reinterpret_cast<Uri*>(&self->setPath(path));
}

static void readUri(Uri* uri, Reader* reader, de::String defaultScheme = "")
{
    Uri_Clear(uri);

    ddstring_t scheme;
    Str_InitStd(&scheme);
    Str_Read(&scheme, reader);

    ddstring_t path;
    Str_InitStd(&path);
    Str_Read(&path, reader);

    if(Str_IsEmpty(&scheme) && !defaultScheme.isEmpty())
    {
        Str_Set(&scheme, defaultScheme.toUtf8().constData());
    }

    Uri_SetScheme(uri, Str_Text(&scheme));
    Uri_SetPath  (uri, Str_Text(&path  ));
}

Uri* Uri_NewWithPath2(char const* path, resourceclassid_t defaultResourceClass)
{
    return reinterpret_cast<Uri*>( new de::Uri(path, defaultResourceClass) );
}

Uri* Uri_NewWithPath(char const* path)
{
    return reinterpret_cast<Uri*>( new de::Uri(path) );
}

Uri* Uri_New(void)
{
    return reinterpret_cast<Uri*>( new de::Uri() );
}

Uri* Uri_Dup(Uri const* other)
{
    DENG_ASSERT(other);
    return reinterpret_cast<Uri*>( new de::Uri(*(TOINTERNAL_CONST(other))) );
}

Uri* Uri_FromReader(Reader* reader)
{
    DENG_ASSERT(reader);

    de::Uri* self = new de::Uri;
    Uri* uri = reinterpret_cast<Uri*>(self);

    readUri(uri, reader);

    return uri;
}

void Uri_Delete(Uri* uri)
{
    if(uri)
    {
        SELF(uri);
        delete self;
    }
}

Uri* Uri_Copy(Uri* uri, Uri const* other)
{
    SELF(uri);
    DENG_ASSERT(other);
    *self = *(TOINTERNAL_CONST(other));
    return reinterpret_cast<Uri*>(self);
}

boolean Uri_Equality(Uri const* uri, Uri const* other)
{
    SELF_CONST(uri);
    DENG_ASSERT(other);
    return *self == (*(TOINTERNAL_CONST(other)));
}

boolean Uri_IsEmpty(Uri const* uri)
{
    SELF_CONST(uri);
    return self->isEmpty();
}

AutoStr* Uri_Resolved(Uri const* uri)
{
    SELF_CONST(uri);
    try
    {
        return AutoStr_FromTextStd(self->resolved().toUtf8().constData());
    }
    catch(de::Uri::ResolveError const& er)
    {
        LOG_WARNING(er.asText());
    }
    return AutoStr_NewStd();
}

const Str* Uri_Scheme(Uri const* uri)
{
    SELF_CONST(uri);
    return self->schemeStr();
}

const Str* Uri_Path(Uri const* uri)
{
    SELF_CONST(uri);
    return self->pathStr();
}

Uri* Uri_SetUri2(Uri* uri, char const* path, resourceclassid_t defaultResourceClass)
{
    SELF(uri);
    return reinterpret_cast<Uri*>(&self->setUri(path, defaultResourceClass));
}

Uri* Uri_SetUri(Uri* uri, char const* path)
{
    SELF(uri);
    return reinterpret_cast<Uri*>(&self->setUri(path));
}

Uri* Uri_SetUriStr(Uri* uri, ddstring_t const* path)
{
    SELF(uri);
    return reinterpret_cast<Uri*>(&self->setUri(Str_Text(path)));
}

AutoStr* Uri_Compose(Uri const* uri)
{
    SELF_CONST(uri);
    return AutoStr_FromTextStd(self->compose().toUtf8().constData());
}

AutoStr* Uri_ToString(Uri const* uri)
{
    SELF_CONST(uri);
    return AutoStr_FromTextStd(self->asText().toUtf8().constData());
}

void Uri_Write2(Uri const* uri, struct writer_s* writer, int omitComponents)
{
    DENG_ASSERT(uri);
    DENG_ASSERT(writer);
    writeUri(uri, writer, omitComponents);
}

void Uri_Write(Uri const* uri, struct writer_s* writer)
{
    DENG_ASSERT(uri);
    DENG_ASSERT(writer);
    writeUri(uri, writer);
}

Uri* Uri_Read(Uri* uri, struct reader_s* reader)
{
    DENG_ASSERT(uri);
    DENG_ASSERT(reader);
    readUri(uri, reader);
    return uri;
}

void Uri_ReadWithDefaultScheme(Uri* uri, struct reader_s* reader, char const* defaultScheme)
{
    DENG_ASSERT(uri);
    DENG_ASSERT(reader);
    readUri(uri, reader, defaultScheme);
}

static de::Uri::PrintFlags translateFlags(int flags)
{
    de::Uri::PrintFlags pf;
    if(flags & UPF_OUTPUT_RESOLVED)           pf |= de::Uri::OutputResolved;
    if(flags & UPF_TRANSFORM_PATH_MAKEPRETTY) pf |= de::Uri::PrettifyPath;
    return pf;
}

void Uri_DebugPrint3(Uri const* uri, int indent, int flags, char const* unresolvedText)
{
    SELF_CONST(uri);
    self->debugPrint(indent, translateFlags(flags), unresolvedText);
}

void Uri_DebugPrint2(Uri const* uri, int indent, int flags)
{
    SELF_CONST(uri);
    self->debugPrint(indent, translateFlags(flags));
}

void Uri_DebugPrint(Uri const* uri, int indent)
{
    SELF_CONST(uri);
    self->debugPrint(indent);
}

DENG_DECLARE_API(Uri) =
{
    { DE_API_URI_latest },
    Uri_New,
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
    Uri_Compose,
    Uri_ToString,
    Uri_Equality,
    Uri_Write2,
    Uri_Write,
    Uri_Read,
    Uri_ReadWithDefaultScheme,
    Uri_DebugPrint3,
    Uri_DebugPrint2,
    Uri_DebugPrint
};
