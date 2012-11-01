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

#include "de_platform.h"
#include "de_console.h"

#include "dd_main.h"
#include "fs_util.h"
#include "game.h"
#include "sys_reslocator.h"

#include <de/Error>
#include <de/Log>
#include <de/String>
#include <de/memory.h>

#include "uri.h"

using namespace de;

struct uri_s
{
    ddstring_t scheme;
    ddstring_t path;

    /// Cached copy of the resolved Uri.
    ddstring_t resolved;

    /// The cached copy only applies when this game is loaded. Add any other
    /// conditions here that result in different results for resolveUri().
    void* resolvedForGame;
};

static void clearCachedResolved(Uri* uri)
{
    Str_Clear(&uri->resolved);
    uri->resolvedForGame = 0;
}

static inline Uri* alloc(char const* path, resourceclass_t defaultResourceClass)
{
    Uri* uri = (Uri*) M_Malloc(sizeof(*uri));
    if(!uri) throw Error("Uri::alloc", String("Failed on allocation of %1 bytes.").arg(sizeof(*uri)));

    Str_Init(&uri->scheme);
    Str_Init(&uri->path);
    Str_InitStd(&uri->resolved);

    uri->resolvedForGame = 0;

    if(path)
    {
        Uri_SetUri3(uri, path, defaultResourceClass);
    }
    return uri;
}

static void parseScheme(Uri* uri, resourceclass_t defaultResourceClass)
{
    LOG_AS("Uri::parseScheme");

    clearCachedResolved(uri);
    Str_Clear(&uri->scheme);

    char const* p = Str_CopyDelim2(&uri->scheme, Str_Text(&uri->path), ':', CDF_OMIT_DELIMITER);
    if(!p || p - Str_Text(&uri->path) < URI_MINSCHEMELENGTH + 1) // +1 for ':' delimiter.
    {
        Str_Clear(&uri->scheme);
    }
    else if(defaultResourceClass != RC_NULL && F_SafeResourceNamespaceForName(Str_Text(Uri_Scheme(uri))) == 0)
    {
        LOG_WARNING("Unknown scheme in path \"%s\", using default.") << Str_Text(&uri->path);
        //Str_Clear(&uri->_scheme);
        Str_Set(&uri->path, p);
    }
    else
    {
        Str_Set(&uri->path, p);
        return;
    }

    // Attempt to guess the scheme by interpreting the path?
    if(defaultResourceClass == RC_UNKNOWN)
        defaultResourceClass = F_DefaultResourceClassForType(F_GuessResourceTypeByName(Str_Text(&uri->path)));

    if(VALID_RESOURCE_CLASS(defaultResourceClass))
    {
        ddstring_t const* name = F_ResourceNamespaceName(F_DefaultResourceNamespaceForClass(defaultResourceClass));
        Str_Copy(&uri->scheme, name);
    }
}

static bool resolveUri(Uri const* uri, ddstring_t* dest)
{
    LOG_AS("Uri::resolve");
    DENG_ASSERT(uri);

    bool successful = false;
    ddstring_t part; Str_Init(&part);

    // Copy the first part of the string as-is up to first '$' if present.
    char const* p;
    if((p = Str_CopyDelim2(dest, Str_Text(&uri->path), '$', CDF_OMIT_DELIMITER)))
    {
        int depth = 0;

        if(*p != '(')
        {
            LOG_WARNING("Invalid character '%c' in \"%s\" at %lu, cannot resolve.")
                << *p << Str_Text(&uri->path) << (p - Str_Text(&uri->path));
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
                    LOG_DEBUG("Cannot resolve 'Game' in \"%s\", no game loaded.") << Str_Text(&uri->path);
                    goto parseEnded;
                }

                Str_Append(dest, Str_Text(&game.identityKey()));
            }
            else if(!Str_CompareIgnoreCase(&part, "GamePlugin.Name"))
            {
                if(!DD_GameLoaded() || !gx.GetVariable)
                {
                    LOG_DEBUG("Cannot resolve 'GamePlugin' in \"%s\", no game plugin loaded.") << Str_Text(&uri->path);
                    goto parseEnded;
                }

                Str_Append(dest, (char*)gx.GetVariable(DD_PLUGIN_NAME));
            }
            else
            {
                LOG_DEBUG("Cannot resolve '%s' in \"%s\", unknown identifier.") << Str_Text(&part) << Str_Text(&uri->path);
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
                LOG_WARNING("Invalid character '%c' in \"%s\" at %lu, cannot resolve.")
                    << *p << Str_Text(&uri->path) << (p - Str_Text(&uri->path));
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

void Uri_Clear(Uri* uri)
{
    DENG_ASSERT(uri);
    Str_Clear(&uri->scheme);
    Str_Clear(&uri->path);
    clearCachedResolved(uri);
}

Uri* Uri_NewWithPath2(char const* path, resourceclass_t defaultResourceClass)
{
    return alloc(path, defaultResourceClass);
}

Uri* Uri_NewWithPath(char const* path)
{
    return alloc(path, RC_UNKNOWN);
}

Uri* Uri_New(void)
{
    return alloc(0, RC_UNKNOWN);
}

Uri* Uri_NewCopy(Uri const* other)
{
    DENG_ASSERT(other);
    Uri* uri = alloc(0, RC_NULL);
    Str_Copy(&uri->scheme, Uri_Scheme(other));
    Str_Copy(&uri->path, Uri_Path(other));
    return uri;
}

Uri* Uri_NewFromReader(struct reader_s* reader)
{
    Uri* uri = Uri_New();
    Uri_Read(uri, reader);
    return uri;
}

void Uri_Delete(Uri* uri)
{
    if(uri)
    {
        Str_Free(&uri->scheme);
        Str_Free(&uri->path);
        Str_Free(&uri->resolved);
        M_Free(uri);
    }
}

Uri* Uri_Copy(Uri* uri, Uri const* other)
{
    DENG_ASSERT(uri);
    DENG_ASSERT(other);

    Str_Copy(&uri->scheme, Uri_Scheme(other));
    Str_Copy(&uri->path,   Uri_Path(other));
    return uri;
}

ddstring_t const* Uri_Scheme(Uri const* uri)
{
    DENG_ASSERT(uri);
    return &uri->scheme;
}

ddstring_t const* Uri_Path(Uri const* uri)
{
    DENG_ASSERT(uri);
    return &uri->path;
}

ddstring_t* Uri_Resolved(Uri const* uri)
{
    ddstring_t const* resolved = Uri_ResolvedConst(uri);
    if(resolved)
    {
        return Str_Set(Str_New(), Str_Text(resolved));
    }
    return 0;
}

ddstring_t const* Uri_ResolvedConst(Uri const* uri)
{
    DENG_ASSERT(uri);
    Uri* modifiable = (Uri*) uri;

#ifndef LIBDENG_DISABLE_URI_RESOLVE_CACHING
    if(uri->resolvedForGame && uri->resolvedForGame == (void*) App_CurrentGame())
    {
        // We can just return the previously prepared resolved URI.
        return &uri->resolved;
    }
#endif

    clearCachedResolved(modifiable);

    // Keep a copy of this, we'll likely need it many, many times.
    if(resolveUri(uri, &modifiable->resolved))
    {
        modifiable->resolvedForGame = (void*) App_CurrentGame();
        return &uri->resolved;
    }
    else
    {
        clearCachedResolved(modifiable);
        return 0;
    }
}

Uri* Uri_SetScheme(Uri* uri, char const* scheme)
{
    DENG_ASSERT(uri);

    Str_Set(&uri->scheme, scheme);
    clearCachedResolved(uri);
    return uri;
}

Uri* Uri_SetPath(Uri* uri, char const* path)
{
    DENG_ASSERT(uri);

    Str_Set(&uri->path, path);
    clearCachedResolved(uri);
    return uri;
}

Uri* Uri_SetUri3(Uri* uri, char const* path, resourceclass_t defaultResourceClass)
{
    LOG_AS("Uri::SetUri");
    DENG_ASSERT(uri);

    if(!path)
    {
        LOG_DEBUG("Attempted with invalid path, will clear.");
        Uri_Clear(uri);
        return uri;
    }

    Str_Set(&uri->path, path);
    Str_Strip(&uri->path);
    parseScheme(uri, defaultResourceClass);
    return uri;
}

Uri* Uri_SetUri2(Uri* uri, char const* path)
{
    return Uri_SetUri3(uri, path, RC_UNKNOWN);
}

Uri* Uri_SetUri(Uri* uri, ddstring_t const* path)
{
    return Uri_SetUri2(uri, path != 0? Str_Text(path) : 0);
}

AutoStr* Uri_Compose(Uri const* uri)
{
    if(!uri) return Str_Set(AutoStr_New(), "(nullptr)");

    AutoStr* out = AutoStr_New();
    if(!Str_IsEmpty(&uri->scheme))
        Str_Appendf(out, "%s:", Str_Text(&uri->scheme));
    Str_Append(out, Str_Text(&uri->path));
    return out;
}

AutoStr* Uri_ToString2(Uri const* uri, boolean percentDecode)
{
    // Just compose it for now, we can worry about making it 'pretty' later.
    AutoStr* path = Uri_Compose(uri);
    if(percentDecode)
        Str_PercentDecode(path);
    return path;
}

AutoStr* Uri_ToString(Uri const* uri)
{
    return Uri_ToString2(uri, true/*apply percent-decoding*/);
}

boolean Uri_Equality(Uri const* uri, Uri const* other)
{
    DENG_ASSERT(uri && other);
    if(uri == other) return true;

    // First, lets check if the scheme differs.
    if(Str_Length(&uri->scheme) != Str_Length(Uri_Scheme(other))) return false;
    if(Str_CompareIgnoreCase(&uri->scheme, Str_Text(Uri_Scheme(other)))) return false;

    // Is resolving not necessary?
    if(!Str_CompareIgnoreCase(&uri->path, Str_Text(Uri_Path(other)))) return true;

    // For comparison purposes we must be able to resolve both paths.
    ddstring_t const* thisPath;
    if(!(thisPath  = Uri_ResolvedConst(uri)))   return false;
    ddstring_t const* otherPath;
    if(!(otherPath = Uri_ResolvedConst(other))) return false;

    // Do not match on partial paths.
    if(Str_Length(thisPath) != Str_Length(otherPath)) return false;

    int result = Str_CompareIgnoreCase(thisPath, Str_Text(otherPath));
    return (result == 0);
}

static void writeUri(ddstring_t const* scheme, ddstring_t const* path, struct writer_s* writer)
{
    Str_Write(scheme, writer);
    Str_Write(path,   writer);
}

void Uri_Write2(Uri const* uri, struct writer_s* writer, int omitComponents)
{
    DENG_ASSERT(uri);
    DENG_ASSERT(writer);

    ddstring_t emptyString;
    ddstring_t const* scheme;

    if(omitComponents & UCF_SCHEME)
    {
        Str_InitStatic(&emptyString, "");
        scheme = &emptyString;
    }
    else
    {
        scheme = &uri->scheme;
    }
    writeUri(scheme, &uri->path, writer);
}

void Uri_Write(Uri const* uri, struct writer_s* writer)
{
    Uri_Write2(uri, writer, 0/*include everything*/);
}

Uri* Uri_Read(Uri* uri, struct reader_s* reader)
{
    DENG_ASSERT(uri);
    DENG_ASSERT(reader);

    Str_Read(&uri->scheme, reader);
    Str_Read(&uri->path,   reader);

    return uri;
}

void Uri_ReadWithDefaultScheme(Uri* uri, struct reader_s* reader, char const* defaultScheme)
{
    Uri_Read(uri, reader);
    if(Str_IsEmpty(&uri->scheme) && defaultScheme)
    {
        Str_Set(&uri->scheme, defaultScheme);
    }
}

void Uri_Print3(Uri const* uri, int indent, int flags, char const* unresolvedText)
{
    DENG_ASSERT(uri);

    indent = MAX_OF(0, indent);
    if(!unresolvedText) unresolvedText = "--(!)incomplete";

    AutoStr* raw = Uri_ToString(uri);

    Con_Printf("%*s\"%s\"", indent, "",
               (flags & UPF_TRANSFORM_PATH_MAKEPRETTY)? F_PrettyPath(Str_Text(raw)) : Str_Text(raw));

    if(flags & UPF_OUTPUT_RESOLVED)
    {
        ddstring_t const* resolved = Uri_ResolvedConst(uri);
        Con_Printf("%s%s\n", (resolved? "=> " : unresolvedText),
                   resolved? ((flags & UPF_TRANSFORM_PATH_MAKEPRETTY)?
                                    F_PrettyPath(Str_Text(resolved)) : Str_Text(resolved))
                           : "");
    }
    Con_Printf("\n");
}

void Uri_Print2(Uri const* uri, int indent, int flags)
{
    Uri_Print3(uri, indent, flags, NULL/*use the default unresolved text*/);
}

void Uri_Print(Uri const* uri, int indent)
{
    Uri_Print2(uri, indent, DEFAULT_PRINTURIFLAGS);
}
