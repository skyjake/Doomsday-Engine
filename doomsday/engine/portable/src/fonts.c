/**\file fonts.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

/**
 * Font Collection
 */

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_system.h"
#include "de_ui.h"

#include "bitmapfont.h"
#include "m_misc.h"

D_CMD(ListFonts);

static int inited = false;

static int numFonts = 0;
static bitmapfont_t** fonts = 0; // The list of fonts.

void Fonts_Register(void)
{
    C_CMD("listfonts", NULL, ListFonts)
}

static void errorIfNotInited(const char* callerName)
{
    if(inited) return;
    Con_Error("%s: fonts module is not presently initialized.", callerName);
    // Unreachable. Prevents static analysers from getting rather confused, poor things.
    exit(1);
}

static fontid_t getNewFontId(void)
{
    int i;
    fontid_t max = 0;
    for(i = 0; i < numFonts; ++i)
    {
        const bitmapfont_t* font = fonts[i];
        if(BitmapFont_Id(font) > max)
            max = BitmapFont_Id(font);
    }
    return max + 1;
}

static fontid_t findFontIdForName(const char* name)
{
    if(name && name[0])
    {
        int i;
        for(i = 0; i < numFonts; ++i)
        {
            const bitmapfont_t* font = fonts[i];
            if(!Str_CompareIgnoreCase(BitmapFont_Name(font), name))
                return BitmapFont_Id(font);
        }
    }
    return 0; // Not found.
}

static int findFontIdxForName(const char* name)
{
    if(name && name[0])
    {
        int i;
        for(i = 0; i < numFonts; ++i)
        {
            const bitmapfont_t* font = fonts[i];
            if(!Str_CompareIgnoreCase(BitmapFont_Name(font), name))
                return i;
        }
    }
    return -1;
}

static int compareFontByName(const void* e1, const void* e2)
{
    return Str_CompareIgnoreCase(BitmapFont_Name(*(const bitmapfont_t**)e1), Str_Text(BitmapFont_Name(*(const bitmapfont_t**)e2)));
}

static int findFontIdxForId(fontid_t id)
{
    if(id != 0)
    {
        int i;
        for(i = 0; i < numFonts; ++i)
        {
            const bitmapfont_t* font = fonts[i];
            if(BitmapFont_Id(font) == id)
                return i;
        }
    }
    return -1;
}

static void destroyFontIdx(int idx)
{
    BitmapFont_Destruct(fonts[idx]);
    memmove(&fonts[idx], &fonts[idx + 1], sizeof(*fonts) * (numFonts - idx - 1));
    --numFonts;
    fonts = realloc(fonts, sizeof(*fonts) * numFonts);
}

static void destroyFonts(void)
{
    while(numFonts) { destroyFontIdx(0); }
    fonts = 0;
    FR_SetNoFont();
}

static bitmapfont_t* addFont(bitmapfont_t* font)
{
    assert(NULL != font);
    fonts = (bitmapfont_t**)realloc(fonts, sizeof(*fonts) * ++numFonts);
    if(NULL == fonts)
        Con_Error("addFont: Failed on (re)allocation of %lu bytes.", (unsigned long)(sizeof(*fonts) * numFonts));
    fonts[numFonts-1] = font;
    return font;
}

/**
 * Creates a new font record for a file and attempts to prepare it.
 */
static bitmapfont_t* loadFont(const char* name, const char* path)
{
    bitmapfont_t* font = addFont(BitmapFont_Construct(getNewFontId(), name, path));
    if(NULL != font)
    {
        VERBOSE( Con_Message("New font '%s'.\n", Str_Text(BitmapFont_Name(font))) )
    }
    return font;
}

int Fonts_Init(void)
{
    if(inited)
        return -1; // No reinitializations...

    inited = true;
    numFonts = 0;
    fonts = 0; // No fonts!

    return 0;
}

void Fonts_Shutdown(void)
{
    if(!inited)
        return;
    destroyFonts();
    inited = false;
}

fontid_t Fonts_CreateFromDef(ded_compositefont_t* def)
{
    bitmapfont_t* font;
    fontid_t id;

    // Valid name?
    if(!def->id[0])
        Con_Error("Fonts_CreateFontFromDef: A valid name is required (not NULL or zero-length).");

    // Already a font by this name?
    if(0 != (id = Fonts_IdForName(def->id)))
    {
        Con_Error("Fonts_CreateFontFromDef: Failed to create font '%s', name already in use.");
        return 0; // Unreachable.
    }

    // A new font.
    font = addFont(BitmapFont_Construct(getNewFontId(), def->id, 0));
    { int i;
    for(i = 0; i < def->charMapCount.num; ++i)
    {
        ddstring_t* path;
        if(!def->charMap[i].path)
            continue;
        if(0 != (path = Uri_Resolved(def->charMap[i].path)))
        {
            BitmapFont_CharSetPatch(font, def->charMap[i].ch, Str_Text(path));
            Str_Delete(path);
        }
    }}
    return BitmapFont_Id(font);
}

fontid_t Fonts_LoadSystemFont(const char* name, const char* searchPath)
{
    assert(name && searchPath && searchPath[0]);
    {
    bitmapfont_t* font;

    errorIfNotInited("Fonts_LoadSystemFont");

    if(0 != findFontIdForName(name) || 0 == F_Access(searchPath))
    {   
        return 0; // Error.
    }

    VERBOSE2( Con_Message("Preparing font \"%s\"...\n", F_PrettyPath(searchPath)) );
    font = loadFont(name, searchPath);
    if(NULL == font)
    {
        Con_Message("Warning: Unknown format for %s\n", searchPath);
        return 0; // Error.
    }

    // Make this the current font.
    FR_SetFont(BitmapFont_Id(font));
    return BitmapFont_Id(font);
    }
}

void Fonts_DestroyFont(fontid_t id)
{
    int idx;
    errorIfNotInited("Fonts_DestroyFont");
    idx = findFontIdxForId(id);
    if(idx == -1) return;

    if(id == FR_Font())
        FR_SetNoFont();

    destroyFontIdx(idx);
}

void Fonts_Update(void)
{
    if(!inited || novideo || isDedicated)
        return;
    { int i;
    for(i = 0; i < numFonts; ++i)
    {
        BitmapFont_DeleteGLTextures(fonts[i]);
        BitmapFont_DeleteGLDisplayLists(fonts[i]);
    }}
}

bitmapfont_t* Fonts_FontForId(fontid_t id)
{
    assert(inited);
    { int idx;
    if(-1 != (idx = findFontIdxForId(id)))
        return fonts[idx];
    }
    return 0;
}

bitmapfont_t* Fonts_FontForIndex(int index)
{
    assert(inited);
    if(index >= 0 && index < numFonts)
        return fonts[index];
    return NULL;
}

/// \note Member of the Doomsday public API.
fontid_t Fonts_IdForName(const char* name)
{
    errorIfNotInited("Fonts_IdForName");
    return findFontIdForName(name);
}

int Fonts_ToIndex(fontid_t id)
{
    int idx;
    errorIfNotInited("Fonts_ToIndex");
    idx = findFontIdxForId(id);
    if(idx == -1)
    {
        Con_Message("Warning:Fonts_ToIndex: Unknown ID %u.\n", id);
    }
    return idx;
}

static int C_DECL compareFontName(const void* a, const void* b)
{
    return Str_CompareIgnoreCase(*((const ddstring_t**)a), Str_Text(*((const ddstring_t**)b)));
}

ddstring_t** Fonts_CollectNames(int* count)
{
    errorIfNotInited("Fonts_CollectNames");
    if(numFonts != 0)
    {
        ddstring_t** list = malloc(sizeof(*list) * (numFonts+1));
        int i;
        for(i = 0; i < numFonts; ++i)
        {
            list[i] = Str_New();
            Str_Copy(list[i], BitmapFont_Name(fonts[i]));
        }
        list[i] = 0; // Terminate.
        qsort(list, numFonts, sizeof(*list), compareFontName);
        if(count)
            *count = numFonts;
        return list;
    }
    if(count)
        *count = 0;
    return 0;
}

static void printFontInfo(const bitmapfont_t* font)
{
    int numDigits = M_NumDigits(numFonts);
    Con_Printf(" %*u: \"system:%s\" ", numDigits, (unsigned int) BitmapFont_Id(font), Str_Text(BitmapFont_Name(font)));
    if(BitmapFont_GLTextureName(font))
    {
        Con_Printf("bitmap (texWidth:%i, texHeight:%i)", BitmapFont_TextureWidth(font), BitmapFont_TextureHeight(font));
    }
    else
    {
       Con_Printf("bitmap_composite"); 
    }
    Con_Printf("\n");
}

static bitmapfont_t** collectFonts(const char* like, size_t* count, bitmapfont_t** storage)
{
    size_t n = 0;
    int i;

    for(i = 0; i < numFonts; ++i)
    {
        bitmapfont_t* font = fonts[i];
        if(like && like[0] && strnicmp(Str_Text(BitmapFont_Name(font)), like, strlen(like)))
            continue;
        if(storage)
            storage[n++] = font;
        else
            ++n;
    }

    if(storage)
    {
        storage[n] = 0; // Terminate.
        if(count)
            *count = n;
        return storage;
    }

    if(n == 0)
    {
        if(count)
            *count = 0;
        return 0;
    }

    storage = malloc(sizeof(bitmapfont_t*) * (n+1));
    return collectFonts(like, count, storage);
}

static size_t printFonts(const char* like)
{
    size_t count = 0;
    bitmapfont_t** foundFonts = collectFonts(like, &count, 0);

    Con_FPrintf(CBLF_YELLOW, "Known Fonts:\n");

    if(!foundFonts)
    {
        Con_Printf(" None found.\n");
        return 0;
    }

    // Print the result index key.
    Con_Printf(" uid: \"(namespace:)name\" font-type\n");
    Con_FPrintf(CBLF_RULER, "");

    // Sort and print the index.
    qsort(foundFonts, count, sizeof(*foundFonts), compareFontByName);

    { bitmapfont_t* const* ptr;
    for(ptr = foundFonts; *ptr; ++ptr)
        printFontInfo(*ptr);
    }

    free(foundFonts);
    return count;
}

D_CMD(ListFonts)
{
    printFonts(argc > 1? argv[1] : NULL);
    return true;
}
