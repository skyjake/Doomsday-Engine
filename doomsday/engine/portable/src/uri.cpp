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
#include "resourcenamespace.h"
#include "sys_reslocator.h"

#include <de/Error>
#include <de/Log>
#include <de/NativePath>
#include <de/String>

#include "uri.h"

extern de::ResourceNamespace* F_ToResourceNamespace(resourcenamespaceid_t rni);

namespace de {

struct Uri::Instance
{
    ddstring_t scheme;
    ddstring_t path;

    /// Cached copy of the resolved Uri.
    String resolved;

    /// The cached copy only applies when this game is loaded. Add any other
    /// conditions here that result in different results for resolveUri().
    void* resolvedForGame;

    Instance()
    {
        Str_InitStd(&scheme);
        Str_InitStd(&path);
        resolvedForGame = 0;
    }

    ~Instance()
    {
        Str_Free(&scheme);
        Str_Free(&path);
    }

    void clearCachedResolved()
    {
        resolved.clear();
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
            ResourceNamespace* rnamespace = F_ToResourceNamespace(F_DefaultResourceNamespaceForClass(defaultResourceClass));
            DENG_ASSERT(rnamespace);
            QByteArray rnamespaceName = rnamespace->name().toUtf8();
            Str_Set(&scheme, rnamespaceName.constData());
        }
    }

    String resolveSymbol(QStringRef const& symbol)
    {
        if(!symbol.compare("App.DataPath", Qt::CaseInsensitive))
        {
            return "data";
        }
        else if(!symbol.compare("App.DefsPath", Qt::CaseInsensitive))
        {
            return "defs";
        }
        else if(!symbol.compare("Game.IdentityKey", Qt::CaseInsensitive))
        {
            de::Game& game = *reinterpret_cast<de::Game*>(App_CurrentGame());
            if(isNullGame(game))
            {
                /// @throw ResolveSymbolError  An unresolveable symbol was encountered.
                throw ResolveSymbolError("Uri::resolveSymbol", "Symbol 'Game' did not resolve (no game loaded)");
            }

            return Str_Text(&game.identityKey());
        }
        else if(!symbol.compare("GamePlugin.Name", Qt::CaseInsensitive))
        {
            if(!DD_GameLoaded() || !gx.GetVariable)
            {
                /// @throw ResolveSymbolError  An unresolveable symbol was encountered.
                throw ResolveSymbolError("Uri::resolveSymbol", "Symbol 'GamePlugin' did not resolve (no game plugin loaded)");
            }

            return String((char*)gx.GetVariable(DD_PLUGIN_NAME));
        }
        else
        {
            /// @throw UnknownSymbolError  An unknown symbol was encountered.
            throw UnknownSymbolError("Uri::resolveSymbol", "Symbol '" + symbol.toString() + "' is unknown");
        }
    }

    inline String parseExpression(QStringRef const& expression)
    {
        // Presently the expression consists of a single symbol.
        return resolveSymbol(expression);
    }

    void resolve(String& result)
    {
        LOG_AS("Uri::resolve");

        String const pathStr = String(Str_Text(&path));

        // Keep scanning the path for embedded expressions.
        QStringRef expression;
        int expEnd = 0, expBegin;
        while((expBegin = pathStr.indexOf('$', expEnd)) >= 0)
        {
            // Is the next char the start-of-expression character?
            if(pathStr.at(expBegin + 1) == '(')
            {
                // Copy everything up to the '$'.
                result += pathStr.mid(expEnd, expBegin - expEnd);

                // Skip over the '$'.
                ++expBegin;

                // Find the end-of-expression character.
                expEnd = pathStr.indexOf(')', expBegin);
                if(expEnd < 0)
                {
                    LOG_WARNING("Missing closing ')' in expression \"" + pathStr + "\", ignoring.");
                    expEnd = pathStr.length();
                }

                // Skip over the '('.
                ++expBegin;

                // The range of the expression substring is now known.
                expression = pathStr.midRef(expBegin, expEnd - expBegin);

                result += parseExpression(expression);
            }
            else
            {
                // No - copy the '$' and continue.
                result += '$';
            }

            ++expEnd;
        }

        // Copy anything remaining.
        result += pathStr.mid(expEnd);
    }
};

Uri::Uri(String path, resourceclass_t defaultResourceClass)
{
    d = new Instance();
    if(!path.isEmpty())
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
    d->resolved = other.d->resolved;
    d->resolvedForGame = other.d->resolvedForGame;
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

    // We must be able to resolve both paths to compare.
    try
    {
        String const& thisPath = resolved();
        String const& otherPath = other.resolved();

        // Do not match partial paths.
        if(thisPath.length() != otherPath.length()) return false;

        return thisPath.compareWithoutCase(otherPath) == 0;
    }
    catch(ResolveError const&)
    {} // Ignore the error.
    return false;
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
#ifdef DENG2_QT_4_8_OR_NEWER
    first.d->resolved.swap(second.d->resolved);
#else
    std::swap(first.d->resolved,        second.d->resolved);
#endif
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

String const& Uri::resolved() const
{
#ifndef LIBDENG_DISABLE_URI_RESOLVE_CACHING
    if(d->resolvedForGame && d->resolvedForGame == (void*) App_CurrentGame())
    {
        // We can just return the previously prepared resolved URI.
        return d->resolved;
    }
#endif

    d->clearCachedResolved();

    String result;
    d->resolve(result);

    // Keep a copy of this, we'll likely need it many, many times.
    d->resolved = result;
    d->resolvedForGame = (void*) App_CurrentGame();

    return d->resolved;
}

Uri& Uri::setScheme(String newScheme)
{
    QByteArray newSchemeUtf8 = newScheme.toUtf8();
    Str_Set(&d->scheme, newSchemeUtf8.constData());
    d->clearCachedResolved();
    return *this;
}

Uri& Uri::setPath(String newPath)
{
    QByteArray newPathUtf8 = newPath.toUtf8();
    Str_Set(&d->path, newPathUtf8.constData());
    d->clearCachedResolved();
    return *this;
}

Uri& Uri::setUri(String rawUri, resourceclass_t defaultResourceClass)
{
    LOG_AS("Uri::setUri");
    QByteArray rawUriUtf8 = rawUri.toUtf8();
    Str_Set(&d->path, rawUriUtf8.constData());
    Str_Strip(&d->path);
    d->parseScheme(defaultResourceClass);
    d->clearCachedResolved();
    return *this;
}

String Uri::compose() const
{
    if(!Str_IsEmpty(&d->scheme))
    {
        return String(Str_Text(&d->scheme)) + ":" + String(Str_Text(&d->path));
    }
    return String(Str_Text(&d->path));
}

String Uri::asText() const
{
    // Just compose it for now, we can worry about making it 'pretty' later.
    QByteArray composed = compose().toUtf8();
    AutoStr* path = AutoStr_FromTextStd(composed.constData());
    Str_PercentDecode(path);
    return String(Str_Text(path));
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

Uri& Uri::read(reader_s& reader, String defaultScheme)
{
    Str_Read(&d->scheme, &reader);
    Str_Read(&d->path,   &reader);
    if(Str_IsEmpty(&d->scheme) && !defaultScheme.isEmpty())
    {
        QByteArray defaultSchemeUtf8 = defaultScheme.toUtf8();
        Str_Set(&d->scheme, defaultSchemeUtf8.constData());
    }
    return *this;
}

void Uri::debugPrint(int indent, int flags, String unresolvedText) const
{
    indent = MAX_OF(0, indent);

    bool resolvedPath = (flags & UPF_OUTPUT_RESOLVED) && !d->resolved.isEmpty();
    if(unresolvedText.isEmpty()) unresolvedText = "--(!)incomplete";

    LOG_DEBUG("%*s\"%s\"%s%s") << indent << ""
      << ((flags & UPF_TRANSFORM_PATH_MAKEPRETTY)? NativePath(asText()).pretty() : asText())
      << ((flags & UPF_OUTPUT_RESOLVED)? (resolvedPath? "=> " : unresolvedText) : "")
      << ((flags & UPF_OUTPUT_RESOLVED) && resolvedPath? ((flags & UPF_TRANSFORM_PATH_MAKEPRETTY)?
                                                                NativePath(d->resolved).pretty() : NativePath(d->resolved))
                                                       : "");
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

AutoStr* Uri_Resolved(Uri const* uri)
{
    SELF_CONST(uri);
    try
    {
        de::String const& resolved = self->resolved();
        QByteArray resolvedUtf8 = resolved.toUtf8();
        return AutoStr_FromTextStd(resolvedUtf8.constData());
    }
    catch(de::Uri::ResolveError const& er)
    {
        LOG_WARNING(er.asText());
    }
    return AutoStr_NewStd();
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
    return reinterpret_cast<Uri*>(&self->setUri(Str_Text(path)));
}

AutoStr* Uri_Compose(Uri const* uri)
{
    SELF_CONST(uri);
    QByteArray composed = self->compose().toUtf8();
    return AutoStr_FromTextStd(composed.constData());
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
