/**
 * @file rawtexture.cpp Raw Texture
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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
#include "de_filesys.h"
#include <de/memoryzone.h>

#include "resource/rawtexture.h"

#define RAWTEX_HASH_SIZE    128
#define RAWTEX_HASH(x)      (rawtexhash + (((unsigned) x) & (RAWTEX_HASH_SIZE - 1)))

struct RawTexHash
{
    rawtex_t* first;
};

static RawTexHash rawtexhash[RAWTEX_HASH_SIZE];

rawtex_t** R_CollectRawTexs(int* count)
{
    rawtex_t** list;

    // First count the number of patchtexs.
    int num = 0;
    for(int i = 0; i < RAWTEX_HASH_SIZE; ++i)
    {
        for(rawtex_t* r = rawtexhash[i].first; r; r = r->next)
            num++;
    }

    // Tell this to the caller.
    if(count) *count = num;

    // Allocate the array, plus one for the terminator.
    list = (rawtex_t**) Z_Malloc(sizeof(**list) * (num + 1), PU_APPSTATIC, NULL);

    // Collect the pointers.
    num = 0;
    for(int i = 0; i < RAWTEX_HASH_SIZE; ++i)
    {
        for(rawtex_t* r = rawtexhash[i].first; r; r = r->next)
            list[num++] = r;
    }

    // Terminate.
    list[num] = NULL;

    return list;
}

rawtex_t* R_FindRawTex(lumpnum_t lumpNum)
{
    LOG_AS("R_FindRawTex");
    if(-1 == lumpNum || lumpNum >= F_LumpCount())
    {
        LOG_DEBUG("LumpNum #%i out of bounds (%i), returning 0.") << lumpNum << F_LumpCount();
        return 0;
    }

    for(rawtex_t* i = RAWTEX_HASH(lumpNum)->first; i; i = i->next)
    {
        if(i->lumpNum == lumpNum)
            return i;
    }
    return 0;
}

rawtex_t *R_GetRawTex(lumpnum_t lumpNum)
{
    LOG_AS("R_GetRawTex");
    if(-1 == lumpNum || lumpNum >= F_LumpCount())
    {
        LOG_DEBUG("LumpNum #%i out of bounds (%i), returning 0.") << lumpNum << F_LumpCount();
        return 0;
    }

    // Check if this lumpNum has already been loaded as a rawtex.
    rawtex_t *r = R_FindRawTex(lumpNum);
    if(r) return r;

    // Hmm, this is an entirely new rawtex.
    r = (rawtex_t *) Z_Calloc(sizeof(*r), PU_REFRESHRAW, 0);
    F_FileName(Str_Init(&r->name), Str_Text(F_LumpName(lumpNum)));
    r->lumpNum = lumpNum;

    // Link to the hash.
    RawTexHash *hash = RAWTEX_HASH(lumpNum);
    r->next = hash->first;
    hash->first = r;

    return r;
}

void R_InitRawTexs()
{
    memset(rawtexhash, 0, sizeof(rawtexhash));
}

void R_UpdateRawTexs()
{
    for(int i = 0; i < RAWTEX_HASH_SIZE; ++i)
    {
        for(rawtex_t *rawTex = rawtexhash[i].first; rawTex; rawTex = rawTex->next)
        {
            Str_Free(&rawTex->name);
        }
    }

    Z_FreeTags(PU_REFRESHRAW, PU_REFRESHRAW);
    R_InitRawTexs();
}
