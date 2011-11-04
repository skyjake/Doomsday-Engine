/**\file resourcerecord.c
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
#include "de_filesys.h"

#include "resourcerecord.h"

static __inline size_t countElements(ddstring_t** list)
{
    size_t n = 0;
    if(list)
    {
        while(list[n++]) { }
    }
    return n;
}

static ddstring_t* buildNameStringList(resourcerecord_t* rec, char delimiter)
{
    int i, requiredLength = 0;
    ddstring_t* list;

    assert(rec);

    if(!rec->_namesCount) return 0;

    for(i = 0; i < rec->_namesCount; ++i)
        requiredLength += Str_Length(rec->_names[i]);
    requiredLength += rec->_namesCount - 1;

    if(!requiredLength) return 0;

    // Build name list in reverse; newer names have precedence.
    list = Str_New();
    Str_Reserve(list, requiredLength);
    Str_Clear(list);

    i = rec->_namesCount-1;
    Str_Set(list, Str_Text(rec->_names[i--]));
    for(; i >= 0; i--)
        Str_Appendf(list, "%c%s", delimiter, Str_Text(rec->_names[i]));

    return list;
}

resourcerecord_t* ResourceRecord_NewWithName(resourceclass_t rclass, int rflags,
    const ddstring_t* name)
{
    resourcerecord_t* rec = (resourcerecord_t*)malloc(sizeof *rec);
    if(!rec)
        Con_Error("ResourceRecord::NewWithName: Failed on allocation of %lu bytes.", (unsigned long) sizeof *rec);

    rec->_rclass = rclass;
    rec->_rflags = rflags;
    rec->_names = 0;
    rec->_namesCount = 0;
    rec->_identityKeys = 0;
    rec->_searchPaths = 0;
    rec->_searchPathUsed = 0;
    Str_Init(&rec->_foundPath);

    ResourceRecord_AddName(rec, name);
    return rec;
}

resourcerecord_t* ResourceRecord_New(resourceclass_t rclass, int rflags)
{
    return ResourceRecord_NewWithName(rclass, rflags, 0);
}

void ResourceRecord_Delete(resourcerecord_t* rec)
{
    assert(rec);
    if(rec->_namesCount != 0)
    {
        int i;
        for(i = 0; i < rec->_namesCount; ++i)
            Str_Delete(rec->_names[i]);
        free(rec->_names);
    }

    if(rec->_identityKeys)
    {
        size_t i;
        for(i = 0; rec->_identityKeys[i]; ++i)
            Str_Delete(rec->_identityKeys[i]);
        free(rec->_identityKeys);
    }

    F_DestroyUriList(rec->_searchPaths);
    Str_Free(&rec->_foundPath);
    free(rec);
}

void ResourceRecord_AddName(resourcerecord_t* rec, const ddstring_t* name)
{
    int i;
    assert(rec);

    if(name == 0 || Str_IsEmpty(name)) return;

    // Is this name unique? We don't want duplicates.
    for(i = 0; i < rec->_namesCount; ++i)
    {
        if(!Str_CompareIgnoreCase(rec->_names[i], Str_Text(name)))
            return;
    }

    // Add the new name.
    rec->_names = (ddstring_t**)realloc(rec->_names, sizeof *rec->_names * ++rec->_namesCount);
    if(!rec->_names)
        Con_Error("ResourceRecord::AddName: Failed on (re)allocation of %lu bytes for names list.", (unsigned long) sizeof *rec->_names * (rec->_namesCount-1));

    rec->_names[rec->_namesCount-1] = Str_New();
    Str_Copy(rec->_names[rec->_namesCount-1], name);

    if(!rec->_searchPaths) return;

    F_DestroyUriList(rec->_searchPaths);
    rec->_searchPaths = 0;
    rec->_searchPathUsed = 0;
    Str_Free(&rec->_foundPath);
}

void ResourceRecord_AddIdentityKey(resourcerecord_t* rec, const ddstring_t* identityKey)
{
    size_t num;
    assert(rec && identityKey);

    num = countElements(rec->_identityKeys);
    rec->_identityKeys = (ddstring_t**)realloc(rec->_identityKeys, sizeof *rec->_identityKeys * MAX_OF(num+1, 2));
    if(!rec->_identityKeys)
        Con_Error("ResourceRecord::AddIdentityKey: Failed on (re)allocation of %lu bytes for identitykey list.", (unsigned long) sizeof *rec->_identityKeys * MAX_OF(num+1, 2));

    if(num) num -= 1;
    rec->_identityKeys[num] = Str_New();
    Str_Copy(rec->_identityKeys[num], identityKey);
    rec->_identityKeys[num+1] = 0; // Terminate.
}

Uri* const* ResourceRecord_SearchPaths(resourcerecord_t* rec)
{
    assert(rec);
    if(!rec->_searchPaths)
    {
        ddstring_t* searchPaths = ResourceRecord_NameStringList(rec);
        rec->_searchPaths = F_CreateUriListStr(rec->_rclass, searchPaths);
        if(searchPaths) Str_Delete(searchPaths);
    }
    return (Uri* const*) rec->_searchPaths;
}

ddstring_t* ResourceRecord_NameStringList(resourcerecord_t* rec)
{
    return buildNameStringList(rec, ';');
}

const ddstring_t* ResourceRecord_ResolvedPath(resourcerecord_t* rec, boolean canLocate)
{
    assert(rec);
    if(rec->_searchPathUsed == 0 && canLocate)
    {
        rec->_searchPathUsed = F_FindResourceForRecord(rec, &rec->_foundPath);
    }
    if(rec->_searchPathUsed != 0)
    {
        return &rec->_foundPath;
    }
    return 0;
}

resourceclass_t ResourceRecord_ResourceClass(resourcerecord_t* rec)
{
    assert(rec);
    return rec->_rclass;
}

int ResourceRecord_ResourceFlags(resourcerecord_t* rec)
{
    assert(rec);
    return rec->_rflags;
}

ddstring_t* const* ResourceRecord_IdentityKeys(resourcerecord_t* rec)
{
    assert(rec);
    return (ddstring_t* const*) rec->_identityKeys;
}

void ResourceRecord_Print(resourcerecord_t* rec, boolean printStatus)
{
    ddstring_t* searchPaths = ResourceRecord_NameStringList(rec);

    if(printStatus)
        Con_Printf("%s", rec->_searchPathUsed == 0? " ! ":"   ");

    Con_PrintPathList4(Str_Text(searchPaths), ';', " or ", PPF_TRANSFORM_PATH_MAKEPRETTY);

    if(printStatus)
    {
        Con_Printf(" %s%s", rec->_searchPathUsed == 0? "- missing" : "- found ",
                   rec->_searchPathUsed == 0? "" : F_PrettyPath(Str_Text(ResourceRecord_ResolvedPath(rec, false))));
    }
    Con_Printf("\n");
    Str_Delete(searchPaths);
}
