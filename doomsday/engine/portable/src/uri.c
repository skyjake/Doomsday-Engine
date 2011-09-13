/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2010-2011 Daniel Swanson <danij@dengine.net>
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

#include "m_args.h"
#include "resourcenamespace.h"
#include "uri.h"

struct uri_s
{
    ddstring_t _scheme;
    ddstring_t _path;
};

static __inline Uri* allocUri(const char* path, resourceclass_t defaultResourceClass)
{
    Uri* uri;
    if((uri = malloc(sizeof(*uri))) == 0)
    {
        Con_Error("Uri::allocUri: Failed on allocation of %lu bytes.", (unsigned long) sizeof(*uri));
        return 0; // Unreachable.
    }
    Str_Init(&uri->_scheme);
    Str_Init(&uri->_path);
    if(path)
        Uri_SetUri3(uri, path, defaultResourceClass);
    return uri;
}

static void parseScheme(Uri* uri, resourceclass_t defaultResourceClass)
{
    Str_Clear(&uri->_scheme);

    { const char* p = Str_CopyDelim2(&uri->_scheme, Str_Text(&uri->_path), ':', CDF_OMIT_DELIMITER);
    if(!p || p - Str_Text(&uri->_path) < URI_MINSCHEMELENGTH+1) // +1 for ':' delimiter.
    {
        Str_Clear(&uri->_scheme);
    }
    else if(defaultResourceClass != RC_NULL && F_SafeResourceNamespaceForName(Str_Text(Uri_Scheme(uri))) == 0)
    {
        Con_Message("Warning: Unknown scheme in path \"%s\", using default.\n", Str_Text(&uri->_path));
        //Str_Clear(&uri->_scheme);
        Str_Set(&uri->_path, p);
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
        Str_Copy(&uri->_scheme, ResourceNamespace_Name(rn));
    }
}


/**
 * Substitute known symbols in the possibly templated path.
 * Resulting path is a well-formed, sys_filein-compatible file path (perhaps base-relative).
 */
static ddstring_t* resolveUri(const Uri* uri)
{
    assert(uri);
    {
    ddstring_t part, *dest = Str_New();
    boolean successful = false;
    const char* p;

    Str_Init(&part);

    // Copy the first part of the string as-is up to first '$' if present.
    if((p = Str_CopyDelim2(dest, Str_Text(&uri->_path), '$', CDF_OMIT_DELIMITER)))
    {
        int depth = 0;

        if(*p != '(')
        {
            Con_Message("Invalid character '%c' in \"%s\" at %lu (Uri::resolveUri).\n",
                *p, Str_Text(&uri->_path), (unsigned long) (p - Str_Text(&uri->_path)));
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
            else if(!Str_CompareIgnoreCase(&part, "GameInfo.DataPath"))
            {
                /// \note DataPath already has ending @c DIR_SEP_CHAR.
                gameinfo_t* info = DD_GameInfo();
                if(DD_IsNullGameInfo(info))
                    goto parseEnded;
                Str_PartAppend(dest, Str_Text(GameInfo_DataPath(info)), 0, Str_Length(GameInfo_DataPath(info))-1);
            }
            else if(!Str_CompareIgnoreCase(&part, "GameInfo.DefsPath"))
            {
                /// \note DefsPath already has ending @c DIR_SEP_CHAR.
                gameinfo_t* info = DD_GameInfo();
                if(DD_IsNullGameInfo(info))
                    goto parseEnded;
                Str_PartAppend(dest, Str_Text(GameInfo_DefsPath(info)), 0, Str_Length(GameInfo_DefsPath(info))-1);
            }
            else if(!Str_CompareIgnoreCase(&part, "GameInfo.IdentityKey"))
            {
                gameinfo_t* info = DD_GameInfo();
                if(DD_IsNullGameInfo(info))
                    goto parseEnded;
                Str_Append(dest, Str_Text(GameInfo_IdentityKey(info)));
            }
            else
            {
                Con_Message("Unknown identifier '%s' in \"%s\" (Uri::resolveUri).\n",
                            Str_Text(&part), Str_Text(&uri->_path));
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
                    *p, Str_Text(&uri->_path), (unsigned long) (p - Str_Text(&uri->_path)));
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

    // Expand any old-style base-relative path directives.
    F_ExpandBasePath(dest, dest);

    // No errors detected.
    successful = true;

parseEnded:
    Str_Free(&part);
    if(!successful && dest)
    {
        Str_Delete(dest);
        return 0;
    }
    return dest;
    }
}

void Uri_Clear(Uri* uri)
{
    if(!uri)
    {
        Con_Error("Attempted Uri::Clear with invalid reference (this==0).");
        return; // Unreachable.
    }
    Str_Clear(&uri->_scheme);
    Str_Clear(&uri->_path);
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
    if(!other)
    {
        Con_Error("Attempted Uri::ConstructCopy with invalid reference (other==0).");
        return 0; // Unreachable.
    }
    { Uri* uri = allocUri(0, RC_NULL);
    Str_Copy(&uri->_scheme, Uri_Scheme(other));
    Str_Copy(&uri->_path, Uri_Path(other));
    return uri;
    }
}

void Uri_Delete(Uri* uri)
{
    if(!uri)
    {
        Con_Error("Attempted Uri::Destruct with invalid reference (this==0).");
        return; // Unreachable.
    }
    Str_Free(&uri->_scheme);
    Str_Free(&uri->_path);
    free(uri);
}

Uri* Uri_Copy(Uri* uri, const Uri* other)
{
    if(!uri)
    {
        Con_Error("Attempted Uri::Copy with invalid reference (this==0).");
        return 0; // Unreachable.
    }
    if(!other)
    {
        Con_Error("Attempted Uri::Copy with invalid reference (other==0).");
        return 0; // Unreachable.
    }
    Str_Copy(&uri->_scheme, Uri_Scheme(other));
    Str_Copy(&uri->_path, Uri_Path(other));
    return uri;
}

const ddstring_t* Uri_Scheme(const Uri* uri)
{
    if(!uri)
    {
        Con_Error("Attempted Uri::Scheme with invalid reference (this==0).");
        return 0; // Unreachable.
    }
    return &uri->_scheme;
}

const ddstring_t* Uri_Path(const Uri* uri)
{
    if(!uri)
    {
        Con_Error("Attempted Uri::Path with invalid reference (this==0).");
        return 0; // Unreachable.
    }
    return &uri->_path;
}

ddstring_t* Uri_Resolved(const Uri* uri)
{
    if(!uri)
    {
        Con_Error("Attempted Uri::Resolved with invalid reference (this==0).");
        return 0; // Unreachable.
    }
    return resolveUri(uri);
}

void Uri_SetScheme(Uri* uri, const char* scheme)
{
    if(!uri)
    {
        Con_Error("Attempted Uri::SetScheme with invalid reference (this==0).");
        return; // Unreachable.
    }
    Str_Set(&uri->_scheme, scheme);
}

void Uri_SetPath(Uri* uri, const char* path)
{
    if(!uri)
    {
        Con_Error("Attempted Uri::SetPath with invalid reference (this==0).");
        return; // Unreachable.
    }
    Str_Set(&uri->_path, path);
}

void Uri_SetUri3(Uri* uri, const char* path, resourceclass_t defaultResourceClass)
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
    F_FixSlashes(&uri->_path, &uri->_path);
    parseScheme(uri, defaultResourceClass);
}

void Uri_SetUri2(Uri* uri, const char* path)
{
    Uri_SetUri3(uri, path, RC_UNKNOWN);
}

void Uri_SetUri(Uri* uri, const ddstring_t* path)
{
    Uri_SetUri2(uri, path != 0? Str_Text(path) : 0);
}

ddstring_t* Uri_ComposePath(const Uri* uri)
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

ddstring_t* Uri_ToString(const Uri* uri)
{
    // Just compose it for now, we can worry about making it 'pretty' later.
    return Uri_ComposePath(uri);
}

boolean Uri_Equality(const Uri* uri, const Uri* other)
{
    assert(uri && other);
    { ddstring_t* thisPath, *otherPath;
    int result;

    if(uri == other)
        return true;

    // First, lets check if the scheme differs.
    if(Str_Length(&uri->_scheme) != Str_Length(Uri_Scheme(other)))
        return false;
    if(Str_CompareIgnoreCase(&uri->_scheme, Str_Text(Uri_Scheme(other))))
        return false;

    if(!Str_CompareIgnoreCase(&uri->_path, Str_Text(Uri_Path(other))))
        return true; // No resolve necessary.

    // For comparison purposes we must be able to resolve both paths.
    if((thisPath = Uri_Resolved(uri)) == 0)
        return false;

    if((otherPath = Uri_Resolved(other)) == 0)
    {
        Str_Delete(thisPath);
        return false;
    }

    // Do not match on partial paths.
    if(Str_Length(thisPath) != Str_Length(otherPath))
    {
        Str_Delete(thisPath);
        Str_Delete(otherPath);
        return false;
    }

    result = Str_CompareIgnoreCase(thisPath, Str_Text(otherPath));

    Str_Delete(thisPath);
    Str_Delete(otherPath);

    return (result == 0);
    }
}
