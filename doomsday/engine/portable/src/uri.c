/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2010 Daniel Swanson <danij@dengine.net>
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

#include "m_args.h"
#include "sys_reslocator.h"
#include "resourcenamespace.h"
#include "dd_uri.h"

static __inline dduri_t* allocUri(const char* path, resourceclass_t defaultResourceClass)
{
    dduri_t* uri;
    if((uri = malloc(sizeof(*uri))) == 0)
    {
        Con_Error("Uri::allocUri: Failed on allocation of %lu bytes for new.", (unsigned long) sizeof(*uri));
        return 0; // Unreachable.
    }
    Str_Init(&uri->_scheme);
    Str_Init(&uri->_path);
    if(path)
        Uri_SetUri3(uri, path, defaultResourceClass);
    return uri;
}

static void parseScheme(dduri_t* uri, resourceclass_t defaultResourceClass)
{
    Str_Clear(&uri->_scheme);

    { const char* p = Str_CopyDelim2(&uri->_scheme, Str_Text(&uri->_path), ':', CDF_OMIT_DELIMITER);
    if(!p || p - Str_Text(&uri->_path) < URI_MINSCHEMELENGTH)
    {
        Str_Clear(&uri->_scheme);
    }
    else if(F_SafeResourceNamespaceForName(Str_Text(Uri_Scheme(uri))) == 0)
    {
        Con_Message("Uri::ParseScheme Unknown namespace \"%s\" in path \"%s\", using default.\n",
                    Str_Text(&uri->_scheme), Str_Text(&uri->_path));
        Str_Clear(&uri->_scheme);
    }
    else
    {
        Str_Set(&uri->_path, p);
        return;
    }}

    // Attempt to guess the scheme by interpreting the path?
    if(defaultResourceClass == RC_UNKNOWN)
        defaultResourceClass = F_DefaultResourceClassForType(F_GuessResourceTypeByName(Str_Text(&uri->_path)));

    if(VALID_RESOURCE_CLASS(defaultResourceClass))
    {
        resourcenamespace_t* rn = F_ToResourceNamespace(F_DefaultResourceNamespaceForClass(defaultResourceClass));
        Str_Set(&uri->_scheme, ResourceNamespace_Name(rn));
    }
}

/**
 * Substitute known symbols in the possibly templated path.
 * Resulting path is a well-formed, sys_filein-compatible file path (perhaps base-relative).
 */
static ddstring_t* resolveUri(const dduri_t* uri, gameinfo_t* info)
{
    assert(uri && info);
    {
    ddstring_t part, src, doomWadDir, *dest = Str_New();
    boolean successful = false;
    const char* p;

    Str_Init(dest);
    Str_Init(&part);
    Str_Init(&doomWadDir);
    Str_Init(&src); Str_Copy(&src, &uri->_path);

    // Copy the first part of the string as-is up to first '$' if present.
    if((p = Str_CopyDelim2(dest, Str_Text(&src), '$', CDF_OMIT_DELIMITER)))
    {
        int depth = 0;

        if(*p != '(')
        {
            Con_Message("Invalid character '%c' in \"%s\" at %lu.\n", *p, Str_Text(&src), p - Str_Text(&src));
            goto parseEnded;
        }
        // Skip over the opening brace.
        p++;
        depth++;

        // Now grab everything up to the closing ')' (it *should* be present).
        while((p = Str_CopyDelim2(&part, p, ')', CDF_OMIT_DELIMITER)))
        {
            // First, try external symbols like environment variable names - they are quick to reject.
            if(!Str_CompareIgnoreCase(&part, "DOOMWADDIR"))
            {
                if(!ArgCheck("-nowaddir") && getenv("DOOMWADDIR"))
                {
                    Str_Set(&doomWadDir, getenv("DOOMWADDIR"));
                    F_FixSlashes(&doomWadDir);
                    if(Str_RAt(&doomWadDir, 0) != DIR_SEP_CHAR)
                        Str_AppendChar(&doomWadDir, DIR_SEP_CHAR);
                    Str_Append(dest, Str_Text(&doomWadDir));
                }
                else
                    goto parseEnded;
            }
            // Now try internal symbols.
            else if(!Str_CompareIgnoreCase(&part, "GameInfo.DataPath"))
            {
                if(DD_IsNullGameInfo(info))
                    goto parseEnded;

                // DataPath already has ending @c DIR_SEP_CHAR.
                Str_PartAppend(dest, Str_Text(GameInfo_DataPath(info)), 0, Str_Length(GameInfo_DataPath(info))-1);
            }
            else if(!Str_CompareIgnoreCase(&part, "GameInfo.DefsPath"))
            {
                if(DD_IsNullGameInfo(info))
                    goto parseEnded;

                // DefsPath already has ending @c DIR_SEP_CHAR.
                Str_PartAppend(dest, Str_Text(GameInfo_DefsPath(info)), 0, Str_Length(GameInfo_DefsPath(info))-1);
            }
            else if(!Str_CompareIgnoreCase(&part, "GameInfo.IdentityKey"))
            {
                if(DD_IsNullGameInfo(info))
                    goto parseEnded;

                Str_Append(dest, Str_Text(GameInfo_IdentityKey(info)));
            }
            else
            {
                Con_Message("Unknown identifier '%s' in \"%s\".\n", Str_Text(&part), Str_Text(&src));
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
                Con_Message("Invalid character '%c' in \"%s\" at %lu.\n", *p, Str_Text(&src), p - Str_Text(&src));
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

    // Expand any base-relative path directives.
    F_ExpandBasePath(dest, dest);

    // No errors detected.
    successful = true;

parseEnded:
    Str_Free(&doomWadDir);
    Str_Free(&part);
    Str_Free(&src);
    if(!successful && dest)
    {
        Str_Delete(dest);
        return 0;
    }
    return dest;
    }
}

void Uri_Clear(dduri_t* uri)
{
    if(!uri)
    {
        Con_Error("Attempted Uri::Clear with invalid reference (this==0).");
        return; // Unreachable.
    }
    Str_Clear(&uri->_scheme);
    Str_Clear(&uri->_path);
}

dduri_t* Uri_Construct2(const char* path, resourceclass_t defaultResourceClass)
{
    return allocUri(path, defaultResourceClass);
}

dduri_t* Uri_Construct(const char* path)
{
    return allocUri(path, RC_UNKNOWN);
}

dduri_t* Uri_ConstructDefault(void)
{
    return allocUri(0, RC_UNKNOWN);
}

void Uri_Destruct(dduri_t* uri)
{
    if(!uri)
    {
        Con_Error("Attempted Uri::Destruct with invalid reference (this==0).");
        return; // Unreachable.
    }
    Str_Free(&uri->_scheme);
    Str_Free(&uri->_path);
}

const ddstring_t* Uri_Scheme(const dduri_t* uri)
{
    if(!uri)
    {
        Con_Error("Attempted Uri::Scheme with invalid reference (this==0).");
        return 0; // Unreachable.
    }
    return &uri->_scheme;
}

const ddstring_t* Uri_Path(const dduri_t* uri)
{
    if(!uri)
    {
        Con_Error("Attempted Uri::Path with invalid reference (this==0).");
        return 0; // Unreachable.
    }
    return &uri->_path;
}

ddstring_t* Uri_Resolved(const dduri_t* uri)
{
    if(!uri)
    {
        Con_Error("Attempted Uri::Resolved with invalid reference (this==0).");
        return 0; // Unreachable.
    }
    return resolveUri(uri, DD_GameInfo());
}

void Uri_SetUri3(dduri_t* uri, const char* path, resourceclass_t defaultResourceClass)
{
    if(!uri)
    {
        Con_Error("Attempted Uri::SetUri with invalid reference (this==0).");
        return; // Unreachable.
    }
    if(!path)
    {
#if _DEBUG
        Con_Message("Attempted Uri::SetUri with invalid reference (@a path==0).\n");
#endif
        Uri_Clear(uri);
        return;
    }
    Str_Set(&uri->_path, path);
    Str_Strip(&uri->_path);
    // Convert all slashes to the host OS's directory separator,
    // for compatibility with the sys_filein routines.
    F_FixSlashes(&uri->_path);
    parseScheme(uri, defaultResourceClass);
}

void Uri_SetUri2(dduri_t* uri, const char* path)
{
    Uri_SetUri3(uri, path, RC_UNKNOWN);
}

void Uri_SetUri(dduri_t* uri, const ddstring_t* path)
{
    Uri_SetUri2(uri, path != 0? Str_Text(path) : 0);
}

ddstring_t* Uri_ComposePath(const dduri_t* uri)
{
    if(!uri)
    {
        Con_Error("Attempted Uri::ComposePath with invalid reference (this==0).");
        return 0; // Unreachable.
    }
    { ddstring_t* out = Str_New(); Str_Init(out);
    if(!Str_IsEmpty(&uri->_scheme))
        Str_Appendf(out, "%s:", Str_Text(&uri->_scheme));
    Str_Append(out, Str_Text(&uri->_path));
    return out;
    }
}

ddstring_t* Uri_ToString(const dduri_t* uri)
{
    // Just compose it for now, we can worry about making it 'pretty' later.
    return Uri_ComposePath(uri);
}
