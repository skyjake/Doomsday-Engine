/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2010-2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_filesys.h"

#include "resourcenamespace.h"
#include "uri.h"

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

static __inline Uri* allocUri(const char* path, resourceclass_t defaultResourceClass)
{
    Uri* uri;
    if((uri = malloc(sizeof(*uri))) == 0)
    {
        Con_Error("Uri::allocUri: Failed on allocation of %lu bytes.", (unsigned long) sizeof(*uri));
        return 0; // Unreachable.
    }
    Str_Init(&uri->scheme);
    Str_Init(&uri->path);
    Str_InitStd(&uri->resolved);
    uri->resolvedForGame = 0;
    if(path)
        Uri_SetUri3(uri, path, defaultResourceClass);
    return uri;
}

static void parseScheme(Uri* uri, resourceclass_t defaultResourceClass)
{
    clearCachedResolved(uri);
    Str_Clear(&uri->scheme);

    { const char* p = Str_CopyDelim2(&uri->scheme, Str_Text(&uri->path), ':', CDF_OMIT_DELIMITER);
    if(!p || p - Str_Text(&uri->path) < URI_MINSCHEMELENGTH+1) // +1 for ':' delimiter.
    {
        Str_Clear(&uri->scheme);
    }
    else if(defaultResourceClass != RC_NULL && F_SafeResourceNamespaceForName(Str_Text(Uri_Scheme(uri))) == 0)
    {
        Con_Message("Warning: Unknown scheme in path \"%s\", using default.\n", Str_Text(&uri->path));
        //Str_Clear(&uri->_scheme);
        Str_Set(&uri->path, p);
    }
    else
    {
        Str_Set(&uri->path, p);
        return;
    }}

    // Attempt to guess the scheme by interpreting the path?
    if(defaultResourceClass == RC_UNKNOWN)
        defaultResourceClass = F_DefaultResourceClassForType(F_GuessResourceTypeByName(Str_Text(&uri->path)));

    if(VALID_RESOURCE_CLASS(defaultResourceClass))
    {
        const ddstring_t* name = F_ResourceNamespaceName(F_DefaultResourceNamespaceForClass(defaultResourceClass));
        Str_Copy(&uri->scheme, name);
    }
}

static boolean resolveUri(const Uri* uri, ddstring_t* dest)
{
    ddstring_t part;
    boolean successful = false;
    const char* p;
    assert(uri);

    Str_Init(&part);

    // Copy the first part of the string as-is up to first '$' if present.
    if((p = Str_CopyDelim2(dest, Str_Text(&uri->path), '$', CDF_OMIT_DELIMITER)))
    {
        int depth = 0;

        if(*p != '(')
        {
            Con_Message("Invalid character '%c' in \"%s\" at %lu (Uri::resolveUri).\n",
                *p, Str_Text(&uri->path), (unsigned long) (p - Str_Text(&uri->path)));
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
            else if(!Str_CompareIgnoreCase(&part, "Game.DataPath"))
            {
                /// \note DataPath already has ending '/'.
                Game* game = theGame;
                if(DD_IsNullGame(game))
                    goto parseEnded;
                Str_PartAppend(dest, Str_Text(Game_DataPath(game)), 0, Str_Length(Game_DataPath(game))-1);
            }
            else if(!Str_CompareIgnoreCase(&part, "Game.DefsPath"))
            {
                /// \note DefsPath already has ending '/'.
                Game* game = theGame;
                if(DD_IsNullGame(game))
                    goto parseEnded;
                Str_PartAppend(dest, Str_Text(Game_DefsPath(game)), 0, Str_Length(Game_DefsPath(game))-1);
            }
            else if(!Str_CompareIgnoreCase(&part, "Game.IdentityKey"))
            {
                Game* game = theGame;
                if(DD_IsNullGame(game))
                    goto parseEnded;
                Str_Append(dest, Str_Text(Game_IdentityKey(game)));
            }
            else
            {
                Con_Message("Unknown identifier '%s' in \"%s\" (Uri::resolveUri).\n",
                            Str_Text(&part), Str_Text(&uri->path));
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
                Con_Message("Invalid character '%c' in \"%s\" at %lu (Uri::resolveUri).\n",
                    *p, Str_Text(&uri->path), (unsigned long) (p - Str_Text(&uri->path)));
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
    if(!uri)
    {
        Con_Error("Attempted Uri::Clear with invalid reference (=NULL).");
        return; // Unreachable.
    }
    Str_Clear(&uri->scheme);
    Str_Clear(&uri->path);
    clearCachedResolved(uri);
}

Uri* Uri_NewWithPath2(const char* path, resourceclass_t defaultResourceClass)
{
    return allocUri(path, defaultResourceClass);
}

Uri* Uri_NewWithPath(const char* path)
{
    return allocUri(path, RC_UNKNOWN);
}

Uri* Uri_New(void)
{
    return allocUri(0, RC_UNKNOWN);
}

Uri* Uri_NewCopy(const Uri* other)
{
    Uri* uri;
    if(!other)
    {
        Con_Error("Attempted Uri::ConstructCopy with invalid reference (other==0).");
        return 0; // Unreachable.
    }

    uri = allocUri(0, RC_NULL);
    Str_Copy(&uri->scheme, Uri_Scheme(other));
    Str_Copy(&uri->path, Uri_Path(other));
    return uri;
}

Uri* Uri_NewFromReader(Reader* reader)
{
    Uri* uri = Uri_New();
    Uri_Read(uri, reader);
    return uri;
}

void Uri_Delete(Uri* uri)
{
    if(!uri)
    {
        Con_Error("Attempted Uri::Delete with invalid reference (=NULL).");
        return; // Unreachable.
    }
    Str_Free(&uri->scheme);
    Str_Free(&uri->path);
    Str_Free(&uri->resolved);
    free(uri);
}

Uri* Uri_Copy(Uri* uri, const Uri* other)
{
    if(!uri)
    {
        Con_Error("Attempted Uri::Copy with invalid reference (=NULL).");
        return 0; // Unreachable.
    }
    if(!other)
    {
        Con_Error("Attempted Uri::Copy with invalid reference (other==0).");
        return 0; // Unreachable.
    }
    Str_Copy(&uri->scheme, Uri_Scheme(other));
    Str_Copy(&uri->path, Uri_Path(other));
    return uri;
}

const ddstring_t* Uri_Scheme(const Uri* uri)
{
    if(!uri)
    {
        Con_Error("Attempted Uri::Scheme with invalid reference (=NULL).");
        exit(1); // Unreachable.
    }
    return &uri->scheme;
}

const ddstring_t* Uri_Path(const Uri* uri)
{
    if(!uri)
    {
        Con_Error("Attempted Uri::Path with invalid reference (=NULL).");
        exit(1); // Unreachable.
    }
    return &uri->path;
}

ddstring_t* Uri_Resolved(const Uri* uri)
{
    const ddstring_t* resolved = Uri_ResolvedConst(uri);
    if(resolved)
    {
        return Str_Set(Str_New(), Str_Text(resolved));
    }
    return 0;
}

const ddstring_t* Uri_ResolvedConst(const Uri* uri)
{
    Uri* modifiable = (Uri*) uri;

    if(!uri)
    {
        Con_Error("Attempted Uri::Resolved with invalid reference (=NULL).");
        exit(1); // Unreachable.
    }

#ifndef LIBDENG_DISABLE_URI_RESOLVE_CACHING
    if(uri->resolvedForGame && uri->resolvedForGame == (void*) theGame)
    {
        // We can just return the previously prepared resolved URI.
        return &uri->resolved;
    }
#endif

    clearCachedResolved(modifiable);

    // Keep a copy of this, we'll likely need it many, many times.
    if(resolveUri(uri, &modifiable->resolved))
    {
        modifiable->resolvedForGame = (void*) theGame;
        return &uri->resolved;
    }
    else
    {
        clearCachedResolved(modifiable);
        return 0;
    }
}

Uri* Uri_SetScheme(Uri* uri, const char* scheme)
{
    if(!uri)
    {
        Con_Error("Attempted Uri::SetScheme with invalid reference (=NULL).");
        exit(1); // Unreachable.
    }
    Str_Set(&uri->scheme, scheme);
    clearCachedResolved(uri);
    return uri;
}

Uri* Uri_SetPath(Uri* uri, const char* path)
{
    if(!uri)
    {
        Con_Error("Attempted Uri::SetPath with invalid reference (=NULL).");
        exit(1); // Unreachable.
    }
    Str_Set(&uri->path, path);
    clearCachedResolved(uri);
    return uri;
}

Uri* Uri_SetUri3(Uri* uri, const char* path, resourceclass_t defaultResourceClass)
{
    if(!uri)
    {
        Con_Error("Attempted Uri::SetUri with invalid reference (=NULL).");
        exit(1); // Unreachable.
    }
    if(!path)
    {
#if _DEBUG
        Con_Message("Attempted Uri::SetUri with invalid reference (@a path=NULL).\n");
#endif
        Uri_Clear(uri);
        return uri;
    }
    Str_Set(&uri->path, path);
    Str_Strip(&uri->path);
    parseScheme(uri, defaultResourceClass);
    return uri;
}

Uri* Uri_SetUri2(Uri* uri, const char* path)
{
    return Uri_SetUri3(uri, path, RC_UNKNOWN);
}

Uri* Uri_SetUri(Uri* uri, const ddstring_t* path)
{
    return Uri_SetUri2(uri, path != 0? Str_Text(path) : 0);
}

ddstring_t* Uri_Compose(const Uri* uri)
{
    ddstring_t* out;
    if(!uri) return Str_Set(Str_New(), "(nullptr)");

    out = Str_New();
    if(!Str_IsEmpty(&uri->scheme))
        Str_Appendf(out, "%s:", Str_Text(&uri->scheme));
    Str_Append(out, Str_Text(&uri->path));
    return out;
}

ddstring_t* Uri_ToString2(const Uri* uri, boolean percentDecode)
{
    // Just compose it for now, we can worry about making it 'pretty' later.
    ddstring_t* path = Uri_Compose(uri);
    if(percentDecode)
        Str_PercentDecode(path);
    return path;
}

ddstring_t* Uri_ToString(const Uri* uri)
{
    return Uri_ToString2(uri, true/*apply percent-decoding*/);
}

boolean Uri_Equality(const Uri* uri, const Uri* other)
{
    assert(uri && other);
    { const ddstring_t* thisPath, *otherPath;
    int result;

    if(uri == other)
        return true;

    // First, lets check if the scheme differs.
    if(Str_Length(&uri->scheme) != Str_Length(Uri_Scheme(other)))
        return false;
    if(Str_CompareIgnoreCase(&uri->scheme, Str_Text(Uri_Scheme(other))))
        return false;

    if(!Str_CompareIgnoreCase(&uri->path, Str_Text(Uri_Path(other))))
        return true; // No resolve necessary.

    // For comparison purposes we must be able to resolve both paths.
    if((thisPath = Uri_ResolvedConst(uri)) == 0)
        return false;

    if((otherPath = Uri_ResolvedConst(other)) == 0)
        return false;

    // Do not match on partial paths.
    if(Str_Length(thisPath) != Str_Length(otherPath))
        return false;

    result = Str_CompareIgnoreCase(thisPath, Str_Text(otherPath));

    return (result == 0);
    }
}

static void writeUri(const ddstring_t* scheme, const ddstring_t* path, Writer* writer)
{
    Str_Write(scheme, writer);
    Str_Write(path, writer);
}

void Uri_Write2(const Uri* uri, Writer* writer, int omitComponents)
{
    ddstring_t emptyString;
    const ddstring_t* scheme;
    assert(uri);
    assert(writer);

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

void Uri_Write(const Uri* uri, Writer* writer)
{
    Uri_Write2(uri, writer, 0/*include everything*/);
}

Uri* Uri_Read(Uri* uri, Reader* reader)
{
    assert(uri);
    assert(reader);

    Str_Read(&uri->scheme, reader);
    Str_Read(&uri->path, reader);

    return uri;
}

void Uri_ReadWithDefaultScheme(Uri* uri, Reader* reader, const char* defaultScheme)
{
    Uri_Read(uri, reader);
    if(Str_IsEmpty(&uri->scheme) && defaultScheme)
    {
        Str_Set(&uri->scheme, defaultScheme);
    }
}

void Uri_Print3(const Uri* uri, int indent, int flags, const char* unresolvedText)
{
    ddstring_t* raw;
    assert(uri);

    indent = MAX_OF(0, indent);
    if(!unresolvedText) unresolvedText = "--(!)incomplete";

    raw = Uri_ToString(uri);

    Con_Printf("%*s\"%s\"", indent, "",
               (flags & UPF_TRANSFORM_PATH_MAKEPRETTY)? F_PrettyPath(Str_Text(raw)) : Str_Text(raw));

    if(flags & UPF_OUTPUT_RESOLVED)
    {
        const ddstring_t* resolved = Uri_ResolvedConst(uri);
        Con_Printf("%s%s\n", (resolved != 0? "=> " : unresolvedText),
                   resolved != 0? ((flags & UPF_TRANSFORM_PATH_MAKEPRETTY)?
                                         F_PrettyPath(Str_Text(resolved)) : Str_Text(resolved))
                                : "");
    }
    Con_Printf("\n");

    Str_Delete(raw);
}

void Uri_Print2(const Uri* uri, int indent, int flags)
{
    Uri_Print3(uri, indent, flags, NULL/*use the default unresolved text*/);
}

void Uri_Print(const Uri* uri, int indent)
{
    Uri_Print2(uri, indent, DEFAULT_PRINTURIFLAGS);
}
