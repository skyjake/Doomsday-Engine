/**
 * @file uri.cpp
 *
 * Universal Resource Identifier.
 *
 * @ingroup base
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

#include "de_base.h"
#include "fs_util.h"
#include "game.h"
#include "sys_reslocator.h"

#include <de/Error>
#include <de/Log>
#include <de/String>

#include "uri.h"

namespace de {

struct Uri::Instance
{
    ddstring_t scheme;
    ddstring_t path;

    /// Cached copy of the resolved Uri.
    ddstring_t resolved;

    /// The cached copy only applies when this game is loaded. Add any other
    /// conditions here that result in different results for resolveUri().
    void* resolvedForGame;

    Instance()
    {
        Str_InitStd(&scheme);
        Str_InitStd(&path);
        Str_InitStd(&resolved);
        resolvedForGame = 0;
    }

    ~Instance()
    {
        Str_Free(&scheme);
        Str_Free(&path);
        Str_Free(&resolved);
    }

    void clearCachedResolved()
    {
        Str_Clear(&resolved);
        resolvedForGame = 0;
    }

    void parseScheme(resourceclass_t defaultResourceClass)
    {
        LOG_AS("Uri::parseScheme");

        clearCachedResolved();
        Str_Clear(&scheme);

        char const* p = Str_CopyDelim2(&scheme, Str_Text(&path), ':', CDF_OMIT_DELIMITER);
        if(!p || p - Str_Text(&path) < URI_MINSCHEMELENGTH + 1) // +1 for ':' delimiter.
        {
            Str_Clear(&scheme);
        }
        else if(defaultResourceClass != RC_NULL && F_SafeResourceNamespaceForName(Str_Text(&scheme)) == 0)
        {
            LOG_WARNING("Unknown scheme in path \"%s\", using default.") << Str_Text(&path);
            //Str_Clear(&_scheme);
            Str_Set(&path, p);
        }
        else
        {
            Str_Set(&path, p);
            return;
        }

        // Attempt to guess the scheme by interpreting the path?
        if(defaultResourceClass == RC_UNKNOWN)
            defaultResourceClass = F_DefaultResourceClassForType(F_GuessResourceTypeByName(Str_Text(&path)));

        if(VALID_RESOURCE_CLASS(defaultResourceClass))
        {
            ddstring_t const* name = F_ResourceNamespaceName(F_DefaultResourceNamespaceForClass(defaultResourceClass));
            Str_Copy(&scheme, name);
        }
    }

    bool resolve(ddstring_t* dest)
    {
        LOG_AS("Uri::resolve");

        bool successful = false;
        ddstring_t part; Str_Init(&part);

        // Copy the first part of the string as-is up to first '$' if present.
        char const* p;
        if((p = Str_CopyDelim2(dest, Str_Text(&path), '$', CDF_OMIT_DELIMITER)))
        {
            int depth = 0;

            if(*p != '(')
            {
                LOG_WARNING("Invalid character '%c' in \"%s\" at %u, cannot resolve.")
                    << *p << Str_Text(&path) << (p - Str_Text(&path));
                goto parseEnded;
            }

            // Skip over the opening brace.
            p++;
            depth++;

            // Now grab everything up to the closing ')' (it *should* be present).
            while((p = Str_CopyDelim2(&part, p, ')', CDF_OMIT_DELIMITER)))
            {
                if(!Str_CompareIgnoreCase(&part, "App.DataPath"))
                {
                    Str_Append(dest, "data");
                }
                else if(!Str_CompareIgnoreCase(&part, "App.DefsPath"))
                {
                    Str_Append(dest, "defs");
                }
                else if(!Str_CompareIgnoreCase(&part, "Game.IdentityKey"))
                {
                    de::Game& game = *reinterpret_cast<de::Game*>(App_CurrentGame());
                    if(isNullGame(game))
                    {
                        LOG_DEBUG("Cannot resolve 'Game' in \"%s\", no game loaded.") << Str_Text(&path);
                        goto parseEnded;
                    }

                    Str_Append(dest, Str_Text(&game.identityKey()));
                }
                else if(!Str_CompareIgnoreCase(&part, "GamePlugin.Name"))
                {
                    if(!DD_GameLoaded() || !gx.GetVariable)
                    {
                        LOG_DEBUG("Cannot resolve 'GamePlugin' in \"%s\", no game plugin loaded.") << Str_Text(&path);
                        goto parseEnded;
                    }

                    Str_Append(dest, (char*)gx.GetVariable(DD_PLUGIN_NAME));
                }
                else
                {
                    LOG_DEBUG("Cannot resolve '%s' in \"%s\", unknown identifier.") << Str_Text(&part) << Str_Text(&path);
                    goto parseEnded;
                }
                depth--;

                // Is there another '$' present?
                if(!(p = Str_CopyDelim2(&part, p, '$', CDF_OMIT_DELIMITER)))
                    break;

                // Copy everything up to the next '$'.
                Str_Append(dest, Str_Text(&part));
                if(*p != '(')
                {
                    LOG_WARNING("Invalid character '%c' in \"%s\" at %u, cannot resolve.")
                        << *p << Str_Text(&path) << (p - Str_Text(&path));
                    goto parseEnded;
                }

                // Skip over the opening brace.
                p++;
                depth++;
            }

            if(depth != 0)
            {
                goto parseEnded;
            }

            // Copy anything remaining.
            Str_Append(dest, Str_Text(&part));
        }

        // No errors detected.
        successful = true;

parseEnded:
        Str_Free(&part);
        return successful;
    }
};

Uri::Uri(char const* path, resourceclass_t defaultResourceClass)
{
    d = new Instance();
    if(path)
    {
        setUri(path, defaultResourceClass);
    }
}

Uri::Uri()
{
    d = new Instance();
}

Uri::Uri(Uri const& other) : LogEntry::Arg::Base()
{
    d = new Instance();
    Str_Copy(&d->scheme, other.scheme());
    Str_Copy(&d->path,   other.path());
}

Uri* Uri::fromReader(struct reader_s& reader)
{
    Uri* uri = new Uri();
    uri->read(reader);
    return uri;
}

Uri::~Uri()
{
    delete d;
}

Uri& Uri::operator = (Uri other)
{
    swap(*this, other);
    return *this;
}

bool Uri::operator == (Uri const& other) const
{
    if(this == &other) return true;

    // First, lets check if the scheme differs.
    if(Str_Length(&d->scheme) != Str_Length(other.scheme())) return false;
    if(Str_CompareIgnoreCase(&d->scheme, Str_Text(other.scheme()))) return false;

    // Is resolving not necessary?
    if(!Str_CompareIgnoreCase(&d->path, Str_Text(other.path()))) return true;

    // For comparison purposes we must be able to resolve both paths.
    ddstring_t const* thisPath;
    if(!(thisPath  = resolvedConst())) return false;
    ddstring_t const* otherPath;
    if(!(otherPath = other.resolvedConst())) return false;

    // Do not match on partial paths.
    if(Str_Length(thisPath) != Str_Length(otherPath)) return false;

    int result = Str_CompareIgnoreCase(thisPath, Str_Text(otherPath));
    return (result == 0);
}

bool Uri::operator != (Uri const& other) const
{
    return !(*this == other);
}

void swap(Uri& first, Uri& second)
{
    /// @todo Is it valid to std::swap a ddstring_t ?
    std::swap(first.d->scheme,          second.d->scheme);
    std::swap(first.d->path,            second.d->path);
    std::swap(first.d->resolved,        second.d->resolved);
    std::swap(first.d->resolvedForGame, second.d->resolvedForGame);
}

Uri& Uri::clear()
{
    Str_Clear(&d->scheme);
    Str_Clear(&d->path);
    d->clearCachedResolved();
    return *this;
}

ddstring_t const* Uri::scheme() const
{
    return &d->scheme;
}

ddstring_t const* Uri::path() const
{
    return &d->path;
}

ddstring_t* Uri::resolved() const
{
    ddstring_t const* resolved = resolvedConst();
    if(resolved)
    {
        return Str_Set(Str_New(), Str_Text(resolved));
    }
    return 0;
}

ddstring_t const* Uri::resolvedConst() const
{
#ifndef LIBDENG_DISABLE_URI_RESOLVE_CACHING
    if(d->resolvedForGame && d->resolvedForGame == (void*) App_CurrentGame())
    {
        // We can just return the previously prepared resolved URI.
        return &d->resolved;
    }
#endif

    d->clearCachedResolved();

    // Keep a copy of this, we'll likely need it many, many times.
    if(d->resolve(&d->resolved))
    {
        d->resolvedForGame = (void*) App_CurrentGame();
        return &d->resolved;
    }
    else
    {
        d->clearCachedResolved();
        return 0;
    }
}

Uri& Uri::setScheme(char const* scheme)
{
    Str_Set(&d->scheme, scheme);
    d->clearCachedResolved();
    return *this;
}

Uri& Uri::setPath(char const* path)
{
    Str_Set(&d->path, path);
    d->clearCachedResolved();
    return *this;
}

Uri& Uri::setUri(char const* path, resourceclass_t defaultResourceClass)
{
    LOG_AS("Uri::SetUri");
    if(!path)
    {
        LOG_DEBUG("Attempted with invalid path, will clear.");
        clear();
        return *this;
    }

    Str_Set(&d->path, path);
    Str_Strip(&d->path);
    d->parseScheme(defaultResourceClass);
    return *this;
}

Uri& Uri::setUri(ddstring_t const* path)
{
    return setUri(path != 0? Str_Text(path) : 0);
}

AutoStr* Uri::compose() const
{
    AutoStr* out = AutoStr_New();
    if(!Str_IsEmpty(&d->scheme))
        Str_Appendf(out, "%s:", Str_Text(&d->scheme));
    Str_Append(out, Str_Text(&d->path));
    return out;
}

String Uri::asText() const
{
    // Just compose it for now, we can worry about making it 'pretty' later.
    AutoStr* path = compose();
    Str_PercentDecode(path);
    return QString::fromUtf8(Str_Text(path));
}

static void writeUri(ddstring_t const* scheme, ddstring_t const* path, struct writer_s& writer)
{
    Str_Write(scheme, &writer);
    Str_Write(path,   &writer);
}

void Uri::write(writer_s& writer, int omitComponents) const
{
    ddstring_t emptyString;
    ddstring_t const* scheme;

    if(omitComponents & UCF_SCHEME)
    {
        Str_InitStatic(&emptyString, "");
        scheme = &emptyString;
    }
    else
    {
        scheme = &d->scheme;
    }
    writeUri(scheme, &d->path, writer);
}

Uri& Uri::read(reader_s& reader, char const* defaultScheme)
{
    Str_Read(&d->scheme, &reader);
    Str_Read(&d->path,   &reader);
    if(Str_IsEmpty(&d->scheme) && defaultScheme)
    {
        Str_Set(&d->scheme, defaultScheme);
    }
    return *this;
}

void Uri::debugPrint(int indent, int flags, char const* unresolvedText) const
{
    indent = MAX_OF(0, indent);

    String raw = asText();
    char const* resolvedPath = (flags & UPF_OUTPUT_RESOLVED)? Str_Text(resolvedConst()) : "";
    if(!unresolvedText) unresolvedText = "--(!)incomplete";

    LOG_DEBUG("%*s\"%s\"%s%s") << indent << ""
      << ((flags & UPF_TRANSFORM_PATH_MAKEPRETTY)? F_PrettyPath(raw.toUtf8().constData()) : raw)
      << ((flags & UPF_OUTPUT_RESOLVED)? (resolvedPath? "=> " : unresolvedText) : "")
      << ((flags & UPF_OUTPUT_RESOLVED) && resolvedPath? ((flags & UPF_TRANSFORM_PATH_MAKEPRETTY)? F_PrettyPath(resolvedPath) : resolvedPath) : "");
}

} // namespace de

/*
 * C Wrapper API:
 */

#define TOINTERNAL(inst) \
    (inst) != 0? reinterpret_cast<de::Uri*>(inst) : NULL

#define TOINTERNAL_CONST(inst) \
    (inst) != 0? reinterpret_cast<de::Uri const*>(inst) : NULL

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    de::Uri* self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    de::Uri const* self = TOINTERNAL_CONST(inst)

Uri* Uri_NewWithPath2(char const* path, resourceclass_t defaultResourceClass)
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

Uri* Uri_FromReader(struct reader_s* reader)
{
    DENG_ASSERT(reader);
    return reinterpret_cast<Uri*>( de::Uri::fromReader(*reader) );
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

Uri* Uri_Clear(Uri* uri)
{
    SELF(uri);
    return reinterpret_cast<Uri*>(&self->clear());
}

ddstring_t const* Uri_Scheme(Uri const* uri)
{
    SELF_CONST(uri);
    return self->scheme();
}

ddstring_t const* Uri_Path(Uri const* uri)
{
    SELF_CONST(uri);
    return self->path();
}

ddstring_t* Uri_Resolved(Uri const* uri)
{
    SELF_CONST(uri);
    return self->resolved();
}

ddstring_t const* Uri_ResolvedConst(Uri const* uri)
{
    SELF_CONST(uri);
    return self->resolvedConst();
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

Uri* Uri_SetUri2(Uri* uri, char const* path, resourceclass_t defaultResourceClass)
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
    return reinterpret_cast<Uri*>(&self->setUri(path));
}

AutoStr* Uri_Compose(Uri const* uri)
{
    SELF_CONST(uri);
    return self->compose();
}

AutoStr* Uri_ToString(Uri const* uri)
{
    SELF_CONST(uri);
    QByteArray text = self->asText().toUtf8();
    return AutoStr_FromTextStd(text.constData());
}

void Uri_Write2(Uri const* uri, struct writer_s* writer, int omitComponents)
{
    SELF_CONST(uri);
    DENG_ASSERT(writer);
    self->write(*writer, omitComponents);
}

void Uri_Write(Uri const* uri, struct writer_s* writer)
{
    SELF_CONST(uri);
    DENG_ASSERT(writer);
    self->write(*writer);
}

Uri* Uri_Read(Uri* uri, struct reader_s* reader)
{
    SELF(uri);
    DENG_ASSERT(reader);
    return reinterpret_cast<Uri*>(&self->read(*reader));
}

void Uri_ReadWithDefaultScheme(Uri* uri, struct reader_s* reader, char const* defaultScheme)
{
    SELF(uri);
    DENG_ASSERT(reader);
    self->read(*reader, defaultScheme);
}

void Uri_DebugPrint3(Uri const* uri, int indent, int flags, char const* unresolvedText)
{
    SELF_CONST(uri);
    self->debugPrint(indent, flags, unresolvedText);
}

void Uri_DebugPrint2(Uri const* uri, int indent, int flags)
{
    SELF_CONST(uri);
    self->debugPrint(indent, flags);
}

void Uri_DebugPrint(Uri const* uri, int indent)
{
    SELF_CONST(uri);
    self->debugPrint(indent);
}
