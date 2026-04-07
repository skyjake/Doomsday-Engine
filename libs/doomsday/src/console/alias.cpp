/** @file
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/console/alias.h"
#include "doomsday/console/knownword.h"
#include <de/legacy/memory.h>

using namespace de;

/// @todo Replace with a BST.
static uint numCAliases;
static calias_t **caliases;

void Con_InitAliases()
{
    numCAliases = 0;
    caliases = 0;
}

void Con_ClearAliases()
{
    if (caliases)
    {
        // Free the alias data.
        calias_t** cal = caliases;
        for (uint i = 0; i < numCAliases; ++i, ++cal)
        {
            M_Free((*cal)->name);
            M_Free((*cal)->command);
            M_Free(*cal);
        }
        M_Free(caliases);
    }
    caliases = 0;
    numCAliases = 0;
}

calias_t *Con_FindAlias(const char *name)
{
    uint bottomIdx, topIdx, pivot;
    calias_t* cal;
    dd_bool isDone;
    int result;

    if (numCAliases == 0) return 0;
    if (!name || !name[0]) return 0;

    bottomIdx = 0;
    topIdx = numCAliases-1;
    cal = NULL;
    isDone = false;
    while (bottomIdx <= topIdx && !isDone)
    {
        pivot = bottomIdx + (topIdx - bottomIdx)/2;

        result = iCmpStrCase(caliases[pivot]->name, name);
        if (result == 0)
        {
            // Found.
            cal = caliases[pivot];
            isDone = true;
        }
        else
        {
            if (result > 0)
            {
                if (pivot == 0)
                {
                    // Not present.
                    isDone = true;
                }
                else
                    topIdx = pivot - 1;
            }
            else
                bottomIdx = pivot + 1;
        }
    }

    return cal;
}

calias_t* Con_AddAlias(char const* name, char const* command)
{
    if (!name || !name[0] || !command || !command[0]) return 0;

    caliases = (calias_t**) M_Realloc(caliases, sizeof(*caliases) * ++numCAliases);

    // Find the insertion point.
    uint idx;
    for (idx = 0; idx < numCAliases-1; ++idx)
    {
        if (iCmpStrCase(caliases[idx]->name, name) > 0)
            break;
    }

    // Make room for the new alias.
    if (idx != numCAliases-1)
        memmove(caliases + idx + 1, caliases + idx, sizeof(*caliases) * (numCAliases - 1 - idx));

    // Add the new variable, making a static copy of the name in the zone (this allows
    // the source data to change in case of dynamic registrations).
    calias_t* newAlias = caliases[idx] = (calias_t*) M_Malloc(sizeof(*newAlias));
    newAlias->name = (char*) M_Malloc(strlen(name) + 1);
    strcpy(newAlias->name, name);
    newAlias->command = (char*) M_Malloc(strlen(command) + 1);
    strcpy(newAlias->command, command);

    Con_UpdateKnownWords();
    return newAlias;
}

void Con_DeleteAlias(calias_t* cal)
{
    DE_ASSERT(cal);

    uint idx;
    for (idx = 0; idx < numCAliases; ++idx)
    {
        if (caliases[idx] == cal)
            break;
    }
    if (idx == numCAliases) return;

    Con_UpdateKnownWords();

    M_Free(cal->name);
    M_Free(cal->command);
    M_Free(cal);

    if (idx < numCAliases - 1)
    {
        memmove(caliases + idx, caliases + idx + 1, sizeof(*caliases) * (numCAliases - idx - 1));
    }
    --numCAliases;
}

String Con_AliasAsStyledText(calias_t *alias)
{
    return String(_E(b)) + alias->name + _E(.) " == " _E(>) + alias->command + _E(<);
}

void Con_AddKnownWordsForAliases()
{
    calias_t** cal = caliases;
    for (uint i = 0; i < numCAliases; ++i, ++cal)
    {
        Con_AddKnownWord(WT_CALIAS, *cal);
    }
}
