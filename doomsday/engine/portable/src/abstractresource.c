/**\file abstractresource.c
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
#include "de_filesys.h"

#include "abstractresource.h"

struct AbstractResource_s {
    /// Class of resource.
    resourceclass_t rclass;

    /// @see resourceFlags.
    int flags;

    /// Array of known potential names from lowest precedence to highest.
    int namesCount;
    ddstring_t** names;

    /// Vector of resource identifier keys (e.g., file or lump names), used for identification purposes.
    ddstring_t** identityKeys;

    /// Paths to use when attempting to locate this resource.
    Uri** searchPaths;

    /// Id+1 of the search path used to locate this resource (in _searchPaths) if found. Set during resource location.
    uint searchPathUsed;

    /// Fully resolved absolute path to the located resource if found. Set during resource location.
    ddstring_t foundPath;
};

static __inline size_t countElements(ddstring_t** list)
{
    size_t n = 0;
    if(list)
    {
        while(list[n++]) { }
    }
    return n;
}

static ddstring_t* buildNameStringList(AbstractResource* r, char delimiter)
{
    int i, requiredLength = 0;
    ddstring_t* list;

    assert(r);

    if(!r->namesCount) return 0;

    for(i = 0; i < r->namesCount; ++i)
        requiredLength += Str_Length(r->names[i]);
    requiredLength += r->namesCount - 1;

    if(!requiredLength) return 0;

    // Build name list in reverse; newer names have precedence.
    list = Str_New();
    Str_Reserve(list, requiredLength);
    Str_Clear(list);

    i = r->namesCount-1;
    Str_Set(list, Str_Text(r->names[i--]));
    for(; i >= 0; i--)
        Str_Appendf(list, "%c%s", delimiter, Str_Text(r->names[i]));

    return list;
}

AbstractResource* AbstractResource_NewWithName(resourceclass_t rclass, int flags,
    const ddstring_t* name)
{
    AbstractResource* r = (AbstractResource*)malloc(sizeof(*r));
    if(!r) Con_Error("AbstractResource::NewWithName: Failed on allocation of %lu bytes.", (unsigned long) sizeof(*r));

    r->rclass = rclass;
    r->flags = flags & ~RF_FOUND;
    r->names = 0;
    r->namesCount = 0;
    r->identityKeys = 0;
    r->searchPaths = 0;
    r->searchPathUsed = 0;
    Str_Init(&r->foundPath);

    AbstractResource_AddName(r, name);
    return r;
}

AbstractResource* AbstractResource_New(resourceclass_t rclass, int rflags)
{
    return AbstractResource_NewWithName(rclass, rflags, 0);
}

void AbstractResource_Delete(AbstractResource* r)
{
    assert(r);
    if(r->namesCount != 0)
    {
        int i;
        for(i = 0; i < r->namesCount; ++i)
            Str_Delete(r->names[i]);
        free(r->names);
    }

    if(r->identityKeys)
    {
        size_t i;
        for(i = 0; r->identityKeys[i]; ++i)
            Str_Delete(r->identityKeys[i]);
        free(r->identityKeys);
    }

    F_DestroyUriList(r->searchPaths);
    Str_Free(&r->foundPath);
    free(r);
}

void AbstractResource_AddName(AbstractResource* r, const ddstring_t* name)
{
    int i;
    assert(r);

    if(name == 0 || Str_IsEmpty(name)) return;

    // Is this name unique? We don't want duplicates.
    for(i = 0; i < r->namesCount; ++i)
    {
        if(!Str_CompareIgnoreCase(r->names[i], Str_Text(name)))
            return;
    }

    // Add the new name.
    r->names = (ddstring_t**)realloc(r->names, sizeof *r->names * ++r->namesCount);
    if(!r->names)
        Con_Error("AbstractResource::AddName: Failed on (re)allocation of %lu bytes for names list.", (unsigned long) sizeof *r->names * r->namesCount);

    r->names[r->namesCount-1] = Str_New();
    Str_Copy(r->names[r->namesCount-1], name);

    if(!r->searchPaths) return;

    F_DestroyUriList(r->searchPaths);
    r->searchPaths = 0;
    r->searchPathUsed = 0;
    Str_Free(&r->foundPath);
}

void AbstractResource_AddIdentityKey(AbstractResource* r, const ddstring_t* identityKey)
{
    size_t num;
    assert(r && identityKey);

    num = countElements(r->identityKeys);
    r->identityKeys = (ddstring_t**)realloc(r->identityKeys, sizeof *r->identityKeys * MAX_OF(num+1, 2));
    if(!r->identityKeys)
        Con_Error("AbstractResource::AddIdentityKey: Failed on (re)allocation of %lu bytes for identitykey list.", (unsigned long) sizeof *r->identityKeys * MAX_OF(num+1, 2));

    if(num) num -= 1;
    r->identityKeys[num] = Str_New();
    Str_Copy(r->identityKeys[num], identityKey);
    r->identityKeys[num+1] = 0; // Terminate.
}

Uri* const* AbstractResource_SearchPaths(AbstractResource* r)
{
    assert(r);
    if(!r->searchPaths)
    {
        ddstring_t* searchPaths = AbstractResource_NameStringList(r);
        r->searchPaths = F_CreateUriListStr(r->rclass, searchPaths);
        if(searchPaths) Str_Delete(searchPaths);
    }
    return (Uri* const*) r->searchPaths;
}

ddstring_t* AbstractResource_NameStringList(AbstractResource* r)
{
    return buildNameStringList(r, ';');
}

const ddstring_t* AbstractResource_ResolvedPath(AbstractResource* r, boolean canLocate)
{
    assert(r);
    if(r->searchPathUsed == 0 && canLocate)
    {
        r->searchPathUsed = F_FindResourceForRecord(r, &r->foundPath);
    }
    /// @todo Move resource validation here.
    if(r->searchPathUsed != 0)
    {
        return &r->foundPath;
    }
    return 0;
}

const ddstring_t* AbstractResource_ResolvedPathWithIndex(AbstractResource* r, int searchPathIndex, boolean canLocate)
{
    const Uri* list[2] = { AbstractResource_SearchPaths(r)[searchPathIndex], NULL };

    assert(r);
    if(canLocate)
    {
        int found = F_FindResourceForRecord2(r, &r->foundPath, list);
        if(found) r->searchPathUsed = searchPathIndex + 1;
    }
    /// @todo Move resource validation here.
    if(r->searchPathUsed != 0)
    {
        return &r->foundPath;
    }
    return 0;
}

resourceclass_t AbstractResource_ResourceClass(AbstractResource* r)
{
    assert(r);
    return r->rclass;
}

int AbstractResource_ResourceFlags(AbstractResource* r)
{
    assert(r);
    return r->flags;
}

AbstractResource* AbstractResource_MarkAsFound(AbstractResource* r, boolean yes)
{
    assert(r);
    if(yes) r->flags |= RF_FOUND;
    else    r->flags &= ~RF_FOUND;
    return r;
}

ddstring_t* const* AbstractResource_IdentityKeys(AbstractResource* r)
{
    assert(r);
    return (ddstring_t* const*) r->identityKeys;
}

void AbstractResource_Print(AbstractResource* r, boolean printStatus)
{
    ddstring_t* searchPaths = AbstractResource_NameStringList(r);
    const boolean markedFound = !!(r->flags & RF_FOUND);

    if(printStatus)
        Con_Printf("%s", !markedFound? " ! ":"   ");

    Con_PrintPathList4(Str_Text(searchPaths), ';', " or ", PPF_TRANSFORM_PATH_MAKEPRETTY);

    if(printStatus)
    {
        Con_Printf(" %s%s", !markedFound? "- missing" : "- found ",
                   !markedFound? "" : F_PrettyPath(Str_Text(AbstractResource_ResolvedPath(r, false))));
    }
    Con_Printf("\n");
    Str_Delete(searchPaths);
}
