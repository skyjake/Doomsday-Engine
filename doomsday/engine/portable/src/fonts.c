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

#include "m_misc.h"

#include "font.h"
#include "bitmapfont.h"

typedef struct fontlist_node_s {
    struct fontlist_node_s* next;
    font_t* font;
} fontlist_node_t;
typedef fontlist_node_t fontlist_t;

typedef struct fontbind_s {
    font_t* _font;
    ddstring_t _name;
    fontnamespaceid_t _namespace;
} fontbind_t;

/// @return  Material associated with this else @c NULL
font_t* FontBind_Font(const fontbind_t* fb);

/// @return  Symbolic name.
const ddstring_t* FontBind_Name(const fontbind_t* fb);

/// @return  Namespace in which this is present.
fontnamespaceid_t FontBind_Namespace(const fontbind_t* fb);

#define FONTNAMESPACE_NAMEHASH_SIZE (512)

typedef struct fontnamespace_namehash_node_s {
    struct fontnamespace_namehash_node_s* next;
    fontnum_t bindId;
} fontnamespace_namehash_node_t;
typedef fontnamespace_namehash_node_t* fontnamespace_namehash_t[FONTNAMESPACE_NAMEHASH_SIZE];

typedef struct fontnamespace_s {
    fontnamespace_namehash_t nameHash;
} fontnamespace_t;

/**
 * The following data structures and variables are intrinsically linked and
 * are inter-dependant. The scheme used is somewhat complicated due to the
 * required traits of the fonts themselves and in of the system itself:
 *
 * 1) Pointers to Fonts are eternal, they are always valid and continue
 *    to reference the same logical font data even after engine reset.
 * 2) Public font identifiers (fontnum_t) are similarly eternal.
 *    Note that they are used to index the font name bindings array.
 * 3) Dynamic creation/update of fonts.
 * 4) Font name bindings are semi-independant from the fonts. There
 *    may be multiple name bindings for a given font (aliases).
 *    The only requirement is that their symbolic names must be unique among
 *    those in the same namespace.
 * 5) Super-fast look up by public font identifier.
 * 6) Fast look up by font name (a hashing scheme is used).
 */
static fontlist_t* fonts;
static fontnum_t bindingsCount;
static fontbind_t* bindings;

static fontnamespace_t namespaces[FONTNAMESPACE_COUNT];

D_CMD(ListFonts);

static int inited = false;

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

static const ddstring_t* nameForFontNamespaceId(fontnamespaceid_t id)
{
    static const ddstring_t emptyString = { "" };
    static const ddstring_t namespaces[FONTNAMESPACE_COUNT] = {
        /* FN_SYSTEM */     { FN_SYSTEM_NAME },
        /* FN_SYSTEM */     { FN_GAME_NAME }
    };
    if(VALID_FONTNAMESPACEID(id))
        return &namespaces[id-FONTNAMESPACE_FIRST];
    return &emptyString;
}

static fontbind_t* bindByIndex(uint bindId)
{
    if(0 != bindId) return &bindings[bindId-1];
    return 0;
}

static int compareStringNames(const void* e1, const void* e2)
{
    return Str_CompareIgnoreCase(*(const ddstring_t**)e1, Str_Text(*(const ddstring_t**)e2));
}

static int compareFontBindByName(const void* e1, const void* e2)
{
    return Str_CompareIgnoreCase(FontBind_Name(*(const fontbind_t**)e1), Str_Text(FontBind_Name(*(const fontbind_t**)e2)));
}

/**
 * This is a hash function. Given a font name it generates a
 * somewhat-random number between 0 and FONTNAMESPACE_NAMEHASH_SIZE.
 *
 * @return  The generated hash index.
 */
static uint hashForName(const char* name)
{
    assert(name);
    {
    uint key = 0;
    byte op = 0;
    const char* c;
    for(c = name; *c; c++)
    {
        switch(op)
        {
        case 0: key ^= tolower(*c); ++op;   break;
        case 1: key *= tolower(*c); ++op;   break;
        case 2: key -= tolower(*c);   op=0; break;
        }
    }
    return key % FONTNAMESPACE_NAMEHASH_SIZE;
    }
}

/**
 * Given a name and namespace, search the fonts db for a match.
 * \assume Caller knows what it's doing; params arn't validity checked.
 *
 * @param name  Name of the font to search for. Must have been transformed
 *      to all lower case.
 * @param namespaceId  Specific FN_* font namespace NOT @c FN_ANY.
 * @return  Unique number of the found font, else zero.
 */
static fontnum_t getFontNumForName(const char* name, uint hash, fontnamespaceid_t namespaceId)
{
    fontnamespace_t* fn = &namespaces[namespaceId-FONTNAMESPACE_FIRST];
    fontnamespace_namehash_node_t* node;
    for(node = (fontnamespace_namehash_node_t*)fn->nameHash[hash];
        NULL != node; node = node->next)
    {
        fontbind_t* fb = bindByIndex(node->bindId);
        if(!strncmp(Str_Text(FontBind_Name(fb)), name, 8))
            return node->bindId;
    }
    return 0; // Not found.
}

static void newFontNameBinding(font_t* font, const char* name,
    fontnamespaceid_t namespaceId, uint hash)
{
    fontnamespace_namehash_node_t* node;
    fontnamespace_t* fn;
    fontbind_t* fb;

    bindings = (fontbind_t*) realloc(bindings, sizeof(*bindings) * ++bindingsCount);
    if(NULL == bindings)
        Con_Error("Fonts::newFontNameBinding: Failed on (re)allocation of %lu bytes for "
            "new FontBind.", (unsigned long) (sizeof(*bindings) * bindingsCount));
    fb = &bindings[bindingsCount - 1];

    Str_Init(&fb->_name); Str_Set(&fb->_name, name);
    fb->_font = font;
    fb->_namespace = namespaceId;

    // We also hash the name for faster searching.
    fn = &namespaces[namespaceId-FONTNAMESPACE_FIRST];
    node = (fontnamespace_namehash_node_t*) malloc(sizeof(*node));
    if(NULL == node)
        Con_Error("Fonts::newFontNameBinding: Failed on allocation of %lu bytes for "
            "new FontNamespace::NameHash::Node.", (unsigned long) sizeof(*node));
    node->next = fn->nameHash[hash];
    node->bindId = bindingsCount; // 1-based index;
    fn->nameHash[hash] = node;

    Font_SetBindId(font, bindingsCount /* 1-based index */);
}

static font_t* constructFont(fonttype_t type)
{
    switch(type)
    {
    case FT_BITMAP:             return BitmapFont_New();
    case FT_BITMAPCOMPOSITE:    return BitmapCompositeFont_New();
    default:
        Con_Error("Fonts::ConstructFont: Unknown font type %i.", (int)type);
        exit(1); // Unreachable.
    }
}

/**
 * Link the font into the global list of fonts.
 * \assume font is NOT already present in the global list.
 */
static font_t* linkFontToGlobalList(font_t* font)
{
    fontlist_node_t* node;
    if(NULL == (node = malloc(sizeof(*node))))
        Con_Error("Fonts::LinkFontToGlobalList: Failed on allocation of %lu bytes for "
            "new FontList::Node.", (unsigned long) sizeof(*node));
    node->font = font;
    node->next = fonts;
    fonts = node;
    return font;
}

static __inline font_t* getFontByIndex(fontnum_t num)
{
    if(num < bindingsCount)
        return FontBind_Font(&bindings[num]);
    Con_Error("Fonts::GetFontByIndex: Invalid index #%u.", (unsigned int) num);
    return NULL; // Unreachable.
}

static void destroyFont(font_t* font)
{
    switch(Font_Type(font))
    {
    case FT_BITMAP:             BitmapFont_Delete(font); return;
    case FT_BITMAPCOMPOSITE:    BitmapCompositeFont_Delete(font); return;
    default:
        Con_Error("Fonts::DestroyFont: Invalid font type %i.", (int) Font_Type(font));
        exit(1); // Unreachable.
    }
}

static void destroyFonts(void)
{
    assert(inited);
    while(NULL != fonts)
    {
        fontlist_node_t* next = fonts->next;
        destroyFont(fonts->font);
        free(fonts);
        fonts = next;
    }
}

static void destroyBindings(void)
{
    assert(inited);

    // Empty the namespace namehash tables.
    { int i;
    for(i = 0; i < FONTNAMESPACE_COUNT; ++i)
    {
        fontnamespace_t* fn = &namespaces[i];
        int j;
        for(j = 0; j < FONTNAMESPACE_NAMEHASH_SIZE; ++j)
        {
            fontnamespace_namehash_node_t* next;
            while(NULL != fn->nameHash[j])
            {
                next = fn->nameHash[j]->next;
                free(fn->nameHash[j]);
                fn->nameHash[j] = next;
            }
        }
    }}

    // Destroy the bindings themselves.
    if(NULL != bindings)
    {
        fontnum_t i;
        for(i = 0; i < bindingsCount; ++i)
        {
            fontbind_t* fb = &bindings[i];
            Str_Free(&fb->_name);
        }
        free(bindings);
        bindings = NULL;
    }
    bindingsCount = 0;
}

font_t* FontBind_Font(const fontbind_t* fb)
{
    assert(fb);
    return fb->_font;
}

const ddstring_t* FontBind_Name(const fontbind_t* fb)
{
    assert(fb);
    return &fb->_name;
}

fontnamespaceid_t FontBind_Namespace(const fontbind_t* fb)
{
    assert(fb);
    return fb->_namespace;
}

void Fonts_Init(void)
{
    if(inited)
        return; // Already been here.

    VERBOSE( Con_Message("Initializing Fonts collection...\n") )

    fonts = NULL;
    bindings = NULL;
    bindingsCount = 0;

    // Clear the name bind hash tables.
    { int i;
    for(i = 0; i < FONTNAMESPACE_COUNT; ++i)
    {
        fontnamespace_t* fn = &namespaces[i];
        memset(fn->nameHash, 0, sizeof(fn->nameHash));
    }}

    inited = true;
}

void Fonts_Shutdown(void)
{
    if(!inited)
        return;

    Fonts_ReleaseRuntimeGLResources();
    Fonts_ReleaseSystemGLResources();

    destroyBindings();
    destroyFonts();
    inited = false;
}

void Fonts_ClearRuntimeFonts(void)
{
    errorIfNotInited("Fonts::ClearRuntimeFonts");
    Fonts_ReleaseRuntimeGLResources();
    //destroyFonts(FN_GAME);
#pragma message("!!!Fonts::ClearRuntimeFonts not yet implemented!!!")
}

void Fonts_ClearSystemFonts(void)
{
    errorIfNotInited("Fonts::ClearSystemFonts");
    Fonts_ReleaseSystemGLResources();
#pragma message("!!!Fonts::ClearSystemFonts not yet implemented!!!")
}

void Fonts_Clear(void)
{
    Fonts_ClearRuntimeFonts();
    Fonts_ClearSystemFonts();
}

void Fonts_ClearDefinitionLinks(void)
{
    errorIfNotInited("Fonts::ClearDefinitionLinks");

    { fontlist_node_t* node;
    for(node = fonts; node; node = node->next)
    {
        font_t* font = node->font;
        if(Font_Type(font) == FT_BITMAPCOMPOSITE)
        {
            BitmapCompositeFont_SetDefinition(font, NULL);
        }
    }}
}

uint Fonts_Count(void)
{
    if(inited)
        return bindingsCount;
    return 0;
}

void Fonts_RebuildBitmapComposite(font_t* font, ded_compositefont_t* def)
{
    assert(NULL != font);

    errorIfNotInited("Fonts::RebuildBitmapComposite");
    if(Font_Type(font) != FT_BITMAPCOMPOSITE)
    {
        Con_Error("Fonts::RebuildBitmapComposite: Font is of invalid type %i.", (int) Font_Type(font));
        exit(1); // Unreachable.
    }

    BitmapCompositeFont_SetDefinition(font, def);
    if(NULL == def)
        return;

    { int i;
    for(i = 0; i < def->charMapCount.num; ++i)
    {
        ddstring_t* path;
        if(!def->charMap[i].path)
            continue;
        if(0 != (path = Uri_Resolved(def->charMap[i].path)))
        {
            BitmapCompositeFont_CharSetPatch(font, def->charMap[i].ch, Str_Text(path));
            Str_Delete(path);
        }
    }}
}

font_t* Fonts_ToFont(fontnum_t num)
{
    errorIfNotInited("Fonts::ToFont");
    if(num != 0 && num <= bindingsCount)
        return getFontByIndex(num - 1); // 1-based index.
    return NULL;
}

fontnum_t Fonts_ToIndex(font_t* font)
{
    errorIfNotInited("Fonts::ToFontNum");
    if(font)
    {
        fontbind_t* fb = bindByIndex(Font_BindId(font));
        if(NULL != fb)
            return (fb - bindings) + 1; // 1-based index.
    }
    return 0;
}

font_t* Fonts_CreateBitmapCompositeFromDef(ded_compositefont_t* def)
{
    assert(def);
    {
    fontnamespaceid_t namespaceId = FN_ANY;
    const dduri_t* uri = def->id;
    font_t* font;
    uint hash;

    errorIfNotInited("Fonts::CreateFromDef");

    if(!uri || Str_IsEmpty(Uri_Path(uri)))
    {
#if _DEBUG
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Warning, attempted to create Font with invalid path \"%s\", ignoring.\n", Str_Text(path));
        Str_Delete(path);
#endif
        return 0;
    }

    hash = hashForName(Str_Text(Uri_Path(uri)));

    namespaceId = DD_ParseFontNamespace(Str_Text(Uri_Scheme(uri)));
    // Check if we've already created a font for this.
    if(!VALID_FONTNAMESPACEID(namespaceId))
    {
#if _DEBUG
        Con_Message("Warning, attempted to create Font in unknown Namespace '%i', ignoring.\n", (int) namespaceId);
#endif
        return NULL;
    }

    { fontnum_t fontNum = getFontNumForName(Str_Text(Uri_Path(uri)), hash, namespaceId);
    if(0 != fontNum)
    {
#if _DEBUG
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Warning, a Font with the path \"%s\" already exists, returning existing.\n", Str_Text(path));
        Str_Delete(path);
#endif
        return getFontByIndex(fontNum);
    }}

    // A new Font.
    font = linkFontToGlobalList(constructFont(FT_BITMAPCOMPOSITE));
    BitmapCompositeFont_SetDefinition(font, def);
    { int i;
    for(i = 0; i < def->charMapCount.num; ++i)
    {
        ddstring_t* path;
        if(!def->charMap[i].path)
            continue;
        if(0 != (path = Uri_Resolved(def->charMap[i].path)))
        {
            BitmapCompositeFont_CharSetPatch(font, def->charMap[i].ch, Str_Text(path));
            Str_Delete(path);
        }
    }}
    newFontNameBinding(font, Str_Text(Uri_Path(uri)), namespaceId, hash);

    // Lets try and prepare it right away.
    BitmapCompositeFont_Prepare(font);

    { ddstring_t* path = Uri_ToString(uri);
    VERBOSE( Con_Message("New font \"%s\".\n", Str_Text(path)) )
    Str_Delete(path);
    }

    return font;
    }
}

/**
 * Creates a new font record for a file and attempts to prepare it.
 */
static font_t* loadExternalFont(const dduri_t* uri, const char* path)
{
    fontnamespaceid_t namespaceId = FN_ANY;
    font_t* font;
    uint hash;

    if(!uri || Str_IsEmpty(Uri_Path(uri)))
    {
#if _DEBUG
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Warning, attempted to create Font with invalid path \"%s\", ignoring.\n", Str_Text(path));
        Str_Delete(path);
#endif
        return 0;
    }

    hash = hashForName(Str_Text(Uri_Path(uri)));

    namespaceId = DD_ParseFontNamespace(Str_Text(Uri_Scheme(uri)));
    // Check if we've already created a font for this.
    if(!VALID_FONTNAMESPACEID(namespaceId))
    {
#if _DEBUG
        Con_Message("Warning, attempted to create Font in unknown Namespace '%i', ignoring.\n", (int) namespaceId);
#endif
        return NULL;
    }

    { fontnum_t fontNum = getFontNumForName(Str_Text(Uri_Path(uri)), hash, namespaceId);
    if(0 != fontNum)
    {
#if _DEBUG
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Warning, a Font with the path \"%s\" already exists, returning existing.\n", Str_Text(path));
        Str_Delete(path);
#endif
        return getFontByIndex(fontNum);
    }}

    // A new Font.
    font = linkFontToGlobalList(constructFont(FT_BITMAP));
    BitmapFont_SetFilePath(font, path);
    newFontNameBinding(font, Str_Text(Uri_Path(uri)), namespaceId, hash);

    // Lets try and prepare it right away.
    BitmapFont_Prepare(font);

    { ddstring_t* path = Uri_ToString(uri);
    VERBOSE( Con_Message("New font \"%s\".\n", Str_Text(path)) )
    Str_Delete(path);
    }

    return font;
}

static fontnum_t Fonts_CheckNumForPath(const dduri_t* uri)
{
    assert(inited && uri);
    {
    const char* name = Str_Text(Uri_Path(uri));
    fontnamespaceid_t namespaceId;
    uint hash;

    if(Str_IsEmpty(Uri_Path(uri)))
        return 0;

    namespaceId = DD_ParseFontNamespace(Str_Text(Uri_Scheme(uri)));
    if(namespaceId != FN_ANY && !VALID_FONTNAMESPACEID(namespaceId))
    {
#if _DEBUG
        Con_Message("Fonts::CheckNumForPath2: Internal error, invalid namespace '%i'\n", (int) namespaceId);
#endif
        return 0;
    }

    hash = hashForName(name);

    if(namespaceId == FN_ANY)
    {   // Caller doesn't care which namespace.
        fontnum_t fontNum;

        // Check for the font in these namespaces, in priority order.
        if((fontNum = getFontNumForName(name, hash, FN_GAME)))
            return fontNum;
        if((fontNum = getFontNumForName(name, hash, FN_SYSTEM)))
            return fontNum;

        return 0; // Not found.
    }

    // Caller wants a font in a specific namespace.
    return getFontNumForName(name, hash, namespaceId);
    }
}

static fontnum_t Fonts_NumForPath(const dduri_t* path)
{
    fontnum_t result;
    if(!inited)
        return 0;
    result = Fonts_CheckNumForPath(path);
    // Not found?
    if(verbose && result == 0)
    {
        ddstring_t* nicePath = Uri_ToString(path);
        Con_Message("Fonts::NumForPath2: \"%s\" not found!\n", Str_Text(nicePath));
        Str_Delete(nicePath);
    }
    return result;
}

static void Fonts_Prepare(font_t* font)
{
    errorIfNotInited("Fonts::Prepare");

    switch(Font_Type(font))
    {
    case FT_BITMAP:             BitmapFont_Prepare(font); break;
    case FT_BITMAPCOMPOSITE:    BitmapCompositeFont_Prepare(font); break;
    default:
        Con_Error("Fonts::Prepare: Invalid font type %i.", (int) Font_Type(font));
        exit(1); // Unreachable.
    }
}

font_t* Fonts_LoadExternal(const char* name, const char* searchPath)
{
    assert(name && searchPath && searchPath[0]);
    {
    fontnum_t fontNum;
    font_t* font = NULL;
    dduri_t* uri = Uri_NewWithPath2(name, RC_NULL);

    errorIfNotInited("Fonts::LoadExternal");

    fontNum = Fonts_CheckNumForPath(uri);
    if(fontNum != 0)
    {
        Uri_Delete(uri);
        return Fonts_ToFont(fontNum);
    }

    if(0 == F_Access(searchPath))
    {   
        Uri_Delete(uri);
        return font; // Error.
    }

    VERBOSE2(
        ddstring_t* path = Uri_ToString(uri);
        Con_Message("Preparing bitmap font \"%s\" as \"%s\"...\n", F_PrettyPath(searchPath), Str_Text(path));
        Str_Delete(path)
    )

    font = loadExternalFont(uri, searchPath);
    if(NULL == font)
    {
        Con_Message("Warning: Unknown format for %s\n", searchPath);
        Uri_Delete(uri);
        return 0; // Error.
    }
    Uri_Delete(uri);

    return font;
    }
}

fontnum_t Fonts_IndexForUri(const dduri_t* path)
{
    if(path)
    {
        return Fonts_CheckNumForPath(path);
    }
    return 0;
}

fontnum_t Fonts_IndexForName(const char* path)
{
    if(path && path[0])
    {
        dduri_t* uri = Uri_NewWithPath2(path, RC_NULL);
        fontnum_t result = Fonts_IndexForUri(uri);
        Uri_Delete(uri);
        return result;
    }
    return 0;
}

/// \note Part of the Doomsday public API.
const ddstring_t* Fonts_GetSymbolicName(font_t* font)
{
    fontnum_t num;
    if(!inited)
        return NULL;
    if(NULL == font)
        return NULL;
    if(0 == (num = Fonts_ToIndex(font)))
        return NULL; // Should never happen. 
    return FontBind_Name(&bindings[num-1]);
}

dduri_t* Fonts_GetUri(font_t* font)
{
    fontbind_t* fb;
    dduri_t* uri;
    ddstring_t path;

    if(!font)
    {
#if _DEBUG
        Con_Message("Warning:Fonts::GetUri: Attempted with invalid reference (font==0), returning 0.\n");
#endif
        return 0;
    }

    Str_Init(&path);
    fb = bindByIndex(Font_BindId(font));
    if(NULL != fb)
    {
        Str_Appendf(&path, "%s:%s", Str_Text(nameForFontNamespaceId(FontBind_Namespace(fb))),
            Str_Text(Fonts_GetSymbolicName(font)));
    }
    uri = Uri_NewWithPath2(Str_Text(&path), RC_NULL);
    Str_Free(&path);
    return uri;
}

void Fonts_ReleaseGLTexturesByNamespace(fontnamespaceid_t namespaceId)
{
    errorIfNotInited("Fonts::ReleaseGLTexturesByNamespace");

    if(namespaceId != FN_ANY && !VALID_FONTNAMESPACEID(namespaceId))
        Con_Error("Fonts::ReleaseGLTexturesByNamespace: Invalid namespace %i.", (int) namespaceId);

    if(novideo || isDedicated)
        return;

    { fontlist_node_t* node;
    for(node = fonts; node; node = node->next)
    {
        font_t* font = node->font;
        
        if(!(namespaceId == FN_ANY || !Font_BindId(font)))
        {
            fontbind_t* fb = bindByIndex(Font_BindId(font));
            if(fb && FontBind_Namespace(fb) != namespaceId)
                continue;
        }

        switch(Font_Type(font))
        {
        case FT_BITMAP:
            BitmapFont_DeleteGLTexture(node->font);
            break;
        case FT_BITMAPCOMPOSITE:
            BitmapCompositeFont_DeleteGLTextures(node->font);
            break;
        default:
            Con_Error("Fonts::ReleaseGLTexturesByNamespace: Invalid font type %i.", (int) Font_Type(font));
            exit(1); // Unreachable.
        }
    }}
}

void Fonts_ReleaseRuntimeGLTextures(void)
{
    errorIfNotInited("Fonts::ReleaseRuntimeGLTextures");
    Fonts_ReleaseGLTexturesByNamespace(FN_GAME);
}

void Fonts_ReleaseSystemGLTextures(void)
{
    errorIfNotInited("Fonts::ReleaseSystemGLTextures");
    Fonts_ReleaseGLTexturesByNamespace(FN_SYSTEM);
}

void Fonts_ReleaseGLResourcesByNamespace(fontnamespaceid_t namespaceId)
{
    errorIfNotInited("Fonts::ReleaseGLResourcesByNamespace");

    if(namespaceId != FN_ANY && !VALID_FONTNAMESPACEID(namespaceId))
        Con_Error("Fonts::ReleaseGLResourcesByNamespace: Invalid namespace %i.", (int) namespaceId);

    if(novideo || isDedicated)
        return;

    { fontlist_node_t* node;
    for(node = fonts; node; node = node->next)
    {
        font_t* font = node->font;
        
        if(!(namespaceId == FN_ANY || !Font_BindId(font)))
        {
            fontbind_t* fb = bindByIndex(Font_BindId(font));
            if(fb && FontBind_Namespace(fb) != namespaceId)
                continue;
        }

        switch(Font_Type(font))
        {
        case FT_BITMAP:
            BitmapFont_DeleteGLTexture(node->font);
            BitmapFont_DeleteGLDisplayLists(node->font);
            break;
        case FT_BITMAPCOMPOSITE:
            BitmapCompositeFont_DeleteGLTextures(node->font);
            BitmapCompositeFont_DeleteGLDisplayLists(node->font);
            break;
        default:
            Con_Error("Fonts::ReleaseGLResourcesByNamespace: Invalid font type %i.", (int) Font_Type(font));
            exit(1); // Unreachable.
        }
    }}
}

void Fonts_ReleaseRuntimeGLResources(void)
{
    errorIfNotInited("Fonts::ReleaseRuntimeTextures");
    Fonts_ReleaseGLTexturesByNamespace(FN_GAME);
}

void Fonts_ReleaseSystemGLResources(void)
{
    errorIfNotInited("Fonts::ReleaseSystemTextures");
    Fonts_ReleaseGLTexturesByNamespace(FN_SYSTEM);
}

int Fonts_Ascent(font_t* font)
{
    Fonts_Prepare(font);
    return Font_Ascent(font);
}

int Fonts_Descent(font_t* font)
{
    Fonts_Prepare(font);
    return Font_Descent(font);
}

int Fonts_Leading(font_t* font)
{
    Fonts_Prepare(font);
    return Font_Leading(font);
}

int Fonts_CharWidth(font_t* font, unsigned char ch)
{
    assert(NULL != font);
    Fonts_Prepare(font);
    switch(Font_Type(font))
    {
    case FT_BITMAP:             return BitmapFont_CharWidth(font, ch);
    case FT_BITMAPCOMPOSITE:    return BitmapCompositeFont_CharWidth(font, ch);
    default:
        Con_Error("Fonts::CharWidth: Invalid font type %i.", (int) Font_Type(font));
        exit(1); // Unreachable.
    }
}

int Fonts_CharHeight(font_t* font, unsigned char ch)
{
    assert(NULL != font);
    Fonts_Prepare(font);
    switch(Font_Type(font))
    {
    case FT_BITMAP:             return BitmapFont_CharHeight(font, ch);
    case FT_BITMAPCOMPOSITE:    return BitmapCompositeFont_CharHeight(font, ch);
    default:
        Con_Error("Fonts::CharHeight: Invalid font type %i.", (int) Font_Type(font));
        exit(1); // Unreachable.
    }
}

void Fonts_CharDimensions(font_t* font, int* width, int* height, unsigned char ch)
{
    assert(NULL != font);
    if(!width && !height)
        return;
    if(width)  *width  = Fonts_CharWidth(font, ch);
    if(height) *height = Fonts_CharHeight(font, ch);
}

ddstring_t** Fonts_CollectNames(int* count)
{
    errorIfNotInited("Fonts::CollectNames");
    if(bindingsCount != 0)
    {
        ddstring_t** list = malloc(sizeof(*list) * (bindingsCount+1));
        uint i;
        for(i = 0; i < bindingsCount; ++i)
        {
            list[i] = Str_New();
            Str_Copy(list[i], FontBind_Name(&bindings[i]));
        }
        list[i] = 0; // Terminate.
        qsort(list, bindingsCount, sizeof(*list), compareStringNames);
        if(count)
            *count = bindingsCount;
        return list;
    }
    if(count)
        *count = 0;
    return 0;
}

static void printFontInfo(const fontbind_t* fb, boolean printNamespace)
{
    int numDigits = M_NumDigits(bindingsCount);
    font_t* font = FontBind_Font(fb);

    Con_Printf(" %*u: \"", numDigits, (unsigned int) Font_BindId(font));
    if(printNamespace)
        Con_Printf("%s:", Str_Text(nameForFontNamespaceId(FontBind_Namespace(fb))));
    Con_Printf("%s\" %s ", Str_Text(FontBind_Name(fb)), Font_Type(font) == FT_BITMAP? "bitmap" : "bitmap_composite");
    if(Font_IsPrepared(font))
    {
        Con_Printf("(ascent:%i, descent:%i, leading:%i", Fonts_Ascent(font), Fonts_Descent(font), Fonts_Leading(font));
        if(Font_Type(font) == FT_BITMAP && BitmapFont_GLTextureName(font))
        {
            Con_Printf(", texWidth:%i, texHeight:%i", BitmapFont_TextureWidth(font), BitmapFont_TextureHeight(font));
        }
        Con_Printf(")\n");
    }
    else
    {
        Con_Printf("\n");
    }
}

static fontbind_t** collectFontBinds(fontnamespaceid_t namespaceId,
    const char* like, size_t* count, fontbind_t** storage)
{
    size_t n = 0;

    if(VALID_FONTNAMESPACEID(namespaceId))
    {
        if(bindings)
        {
            fontnamespace_t* fn = &namespaces[namespaceId-FONTNAMESPACE_FIRST];
            int i;
            for(i = 0; i < FONTNAMESPACE_NAMEHASH_SIZE; ++i)
            {
                fontnamespace_namehash_node_t* node;

                if(NULL == fn->nameHash[i]) continue;

                for(node = (fontnamespace_namehash_node_t*) fn->nameHash[i];
                    NULL != node; node = node->next)
                {
                    fontbind_t* fb = bindByIndex(node->bindId);
                    if(!(like && like[0] && strnicmp(Str_Text(FontBind_Name(fb)), like, strlen(like))))
                    {
                        if(storage)
                            storage[n++] = fb;
                        else
                            ++n;
                    }
                }
            }
        }
    }
    else
    {
        // Any.
        fontbind_t* fb = bindings;
        fontnum_t i = 0;
        for(; i < bindingsCount; ++i, ++fb)
        {
            if(like && like[0] && strnicmp(Str_Text(FontBind_Name(fb)), like, strlen(like)))
                continue;
            if(storage)
                storage[n++] = fb;
            else
                ++n;
        }
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

    storage = malloc(sizeof(fontbind_t*) * (n+1));
    return collectFontBinds(namespaceId, like, count, storage);
}

static size_t printFonts2(fontnamespaceid_t namespaceId, const char* like)
{
    size_t count = 0;
    fontbind_t** foundFonts = collectFontBinds(namespaceId, like, &count, 0);

    if(VALID_FONTNAMESPACEID(namespaceId))
        Con_FPrintf(CBLF_YELLOW, "Known Fonts in \"%s\":\n", Str_Text(nameForFontNamespaceId(namespaceId)));
    else // Any namespace.
        Con_FPrintf(CBLF_YELLOW, "Known Fonts:\n");

    if(!foundFonts)
    {
        Con_Printf(" None found.\n");
        return 0;
    }

    // Print the result index key.
    Con_Printf(" uid: \"%s\" font-type", VALID_FONTNAMESPACEID(namespaceId)? "font-name" : "<namespace>:font-name");
    // Fonts may be prepared only if GL is inited thus if we can't prepare, we can't list property values.
    if(GL_IsInited())
    {
        Con_Printf(" (<property-name>:<value>, ...)");
    }
    Con_Printf("\n");
    Con_FPrintf(CBLF_RULER, "");

    // Sort and print the index.
    qsort(foundFonts, count, sizeof(*foundFonts), compareFontBindByName);

    { fontbind_t* const* ptr;
    for(ptr = foundFonts; *ptr; ++ptr)
        printFontInfo(*ptr, (namespaceId == FN_ANY));
    }

    free(foundFonts);
    return count;
}

static void printFonts(fontnamespaceid_t namespaceId, const char* like)
{
    // Only one namespace to print?
    if(VALID_FONTNAMESPACEID(namespaceId))
    {
        printFonts2(namespaceId, like);
        return;
    }

    // Collect and sort in each namespace separately.
    { int i;
    for(i = FONTNAMESPACE_FIRST; i <= FONTNAMESPACE_LAST; ++i)
    {
        if(printFonts2((fontnamespaceid_t)i, like) != 0)
            Con_FPrintf(CBLF_RULER, "");
    }}
}

D_CMD(ListFonts)
{
    fontnamespaceid_t namespaceId = (argc > 1? DD_ParseFontNamespace(argv[1]) : FN_ANY);
    if(argc > 2 && !VALID_FONTNAMESPACEID(namespaceId))
    {
        Con_Printf("Unknown font namespace \"%s\".\n", argv[1]);
        return false;
    }
    printFonts(namespaceId, (argc > 2? argv[2] : (argc > 1 && !VALID_FONTNAMESPACEID(namespaceId)? argv[1] : NULL)));
    return true;
}
